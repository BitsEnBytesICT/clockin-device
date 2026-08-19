#include "services/api/ApiWorker.hpp"

#include "services/api/ApiClient.hpp"

#include <utility>

namespace clockin {

ApiWorker::ApiWorker(std::string base_url, std::string api_key)
    : base_url_(std::move(base_url)), api_key_(std::move(api_key)) {}

ApiWorker::~ApiWorker() { Stop(); }

bool ApiWorker::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return true;
    stopping_.store(false);
    try {
        thread_ = std::thread(&ApiWorker::ThreadMain, this);
    } catch (...) {
        running_.store(false);
        return false;
    }
    return true;
}

void ApiWorker::Stop() {
    if (!running_.load()) return;
    stopping_.store(true);
    condition_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.clear();
    running_.store(false);
}

std::uint64_t ApiWorker::SubmitHealth() {
    ApiJob job;
    job.type = ApiJobType::Health;
    return Submit(std::move(job), false);
}

std::uint64_t ApiWorker::SubmitScan(const std::string& uid) {
    ApiJob job;
    job.type = ApiJobType::Scan;
    job.rfid_uid = uid;
    return Submit(std::move(job), true);
}

std::uint64_t ApiWorker::SubmitAttendance(const std::string& uid) {
    ApiJob job;
    job.type = ApiJobType::Attendance;
    job.rfid_uid = uid;
    return Submit(std::move(job), true);
}

std::uint64_t ApiWorker::SubmitSignature(const std::string& uid,
                                         const std::string& signature) {
    ApiJob job;
    job.type = ApiJobType::Signature;
    job.rfid_uid = uid;
    job.payload = signature;
    return Submit(std::move(job), true);
}

bool ApiWorker::TryPop(ApiResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.empty()) return false;
    result = std::move(results_.front());
    results_.pop_front();
    return true;
}

bool ApiWorker::HasApiKey() const { return !api_key_.empty(); }

const std::string& ApiWorker::BaseUrl() const { return base_url_; }

std::uint64_t ApiWorker::Submit(ApiJob job, bool priority) {
    if (!running_.load() || stopping_.load()) return 0;
    const std::uint64_t assigned_id = next_id_.fetch_add(1);
    job.id = assigned_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (jobs_.size() >= kMaxQueuedJobs) return 0;
        if (priority) jobs_.push_front(std::move(job));
        else jobs_.push_back(std::move(job));
    }
    condition_.notify_one();
    return assigned_id;
}

void ApiWorker::ThreadMain() {
    ApiClient client(base_url_, api_key_);
    while (!stopping_.load()) {
        ApiJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stopping_.load() || !jobs_.empty(); });
            if (stopping_.load()) break;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        ApiResult result = client.Execute(job, stopping_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (results_.size() >= kMaxResults) results_.pop_front();
        results_.push_back(std::move(result));
    }
}

}  // namespace clockin
