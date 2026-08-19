#ifndef API_WORKER_H
#define API_WORKER_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>

#include "api_client.h"
#include "api_types.h"

class APIWorker {
public:
    APIWorker()
        : config_(APIClient::ResolveConfig()),
          stopping_(false),
          running_(false),
          next_id_(1) {}

    ~APIWorker() { Stop(); }

    bool Start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) return true;
        stopping_.store(false);
        try {
            thread_ = std::thread(&APIWorker::ThreadMain, this);
        } catch (...) {
            running_.store(false);
            return false;
        }
        return true;
    }

    void Stop() {
        if (!running_.load()) return;
        stopping_.store(true);
        condition_.notify_all();
        if (thread_.joinable()) thread_.join();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.clear();
        }
        running_.store(false);
    }

    uint64_t SubmitHealth() {
        ApiJob job;
        job.type = ApiJobType::Health;
        return Submit(job, false);
    }

    uint64_t SubmitScan(const std::string& uid) {
        ApiJob job;
        job.type = ApiJobType::Scan;
        job.rfid_uid = uid;
        return Submit(job, true);
    }

    uint64_t SubmitAttendance(const std::string& uid) {
        ApiJob job;
        job.type = ApiJobType::Attendance;
        job.rfid_uid = uid;
        return Submit(job, true);
    }

    uint64_t SubmitSignature(const std::string& uid, const std::string& signature) {
        ApiJob job;
        job.type = ApiJobType::Signature;
        job.rfid_uid = uid;
        job.payload = signature;
        return Submit(job, true);
    }

    bool TryPop(ApiResult& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (results_.empty()) return false;
        result = std::move(results_.front());
        results_.pop_front();
        return true;
    }

    bool IsMockMode() const { return config_.mock_api; }
    bool HasAPIKey() const { return !config_.api_key.empty(); }
    const std::string& GetBaseURL() const { return config_.base_url; }

private:
    static const size_t MAX_QUEUED_JOBS = 4;
    static const size_t MAX_RESULTS = 8;

    APIClient::Config config_;
    std::atomic<bool> stopping_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_id_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<ApiJob> jobs_;
    std::deque<ApiResult> results_;

    uint64_t Submit(ApiJob& job, bool priority) {
        if (!running_.load() || stopping_.load()) return 0;
        job.id = next_id_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (jobs_.size() >= MAX_QUEUED_JOBS) return 0;
            if (priority) {
                jobs_.push_front(job);
            } else {
                jobs_.push_back(job);
            }
        }
        condition_.notify_one();
        return job.id;
    }

    void ThreadMain() {
        APIClient client(config_);
        while (!stopping_.load()) {
            ApiJob job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return stopping_.load() || !jobs_.empty(); });
                if (stopping_.load()) break;
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }

            ApiResult result = client.Execute(job, stopping_);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (results_.size() >= MAX_RESULTS) results_.pop_front();
                results_.push_back(std::move(result));
            }
        }
    }
};

#endif
