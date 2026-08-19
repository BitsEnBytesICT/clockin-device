#pragma once

#include "services/api/IApiService.hpp"

#include <deque>
#include <utility>
#include <vector>

namespace clockin::test {

class FakeApiService final : public IApiService {
public:
    bool Start() override {
        running = start_result;
        return start_result;
    }
    void Stop() override { running = false; }
    std::uint64_t SubmitHealth() override { return Submit(ApiJobType::Health, {}, {}); }
    std::uint64_t SubmitScan(const std::string& uid) override {
        return Submit(ApiJobType::Scan, uid, {});
    }
    std::uint64_t SubmitAttendance(const std::string& uid) override {
        return Submit(ApiJobType::Attendance, uid, {});
    }
    std::uint64_t SubmitSignature(const std::string& uid,
                                  const std::string& signature) override {
        return Submit(ApiJobType::Signature, uid, signature);
    }
    bool TryPop(ApiResult& result) override {
        if (results.empty()) return false;
        result = std::move(results.front());
        results.pop_front();
        return true;
    }
    bool HasApiKey() const override { return has_api_key; }
    const std::string& BaseUrl() const override { return base_url; }

    std::uint64_t LastId(ApiJobType type) const {
        for (auto iterator = jobs.rbegin(); iterator != jobs.rend(); ++iterator) {
            if (iterator->type == type) return iterator->id;
        }
        return 0;
    }

    std::size_t Count(ApiJobType type) const {
        std::size_t count = 0;
        for (const ApiJob& job : jobs) {
            if (job.type == type) ++count;
        }
        return count;
    }

    void Push(ApiResult result) { results.push_back(std::move(result)); }

    bool start_result = true;
    bool running = false;
    bool has_api_key = true;
    std::string base_url = "http://test.invalid";
    std::vector<ApiJob> jobs;
    std::deque<ApiResult> results;

private:
    std::uint64_t next_id_ = 1;

    std::uint64_t Submit(ApiJobType type,
                         const std::string& uid,
                         const std::string& payload) {
        if (!running) return 0;
        ApiJob job;
        job.id = next_id_++;
        job.type = type;
        job.rfid_uid = uid;
        job.payload = payload;
        jobs.push_back(job);
        return job.id;
    }
};

}  // namespace clockin::test
