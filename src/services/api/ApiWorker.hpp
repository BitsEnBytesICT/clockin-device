#pragma once

#include "services/api/IApiService.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace clockin {

class ApiWorker final : public IApiService {
public:
    ApiWorker(std::string base_url, std::string api_key);
    ~ApiWorker() override;

    bool Start() override;
    void Stop() override;
    std::uint64_t SubmitHealth() override;
    std::uint64_t SubmitScan(const std::string& uid) override;
    std::uint64_t SubmitAttendance(const std::string& uid) override;
    std::uint64_t SubmitSignature(const std::string& uid,
                                  const std::string& signature) override;
    bool TryPop(ApiResult& result) override;
    bool HasApiKey() const override;
    const std::string& BaseUrl() const override;

private:
    static constexpr std::size_t kMaxQueuedJobs = 4;
    static constexpr std::size_t kMaxResults = 8;

    std::string base_url_;
    std::string api_key_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> next_id_{1};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<ApiJob> jobs_;
    std::deque<ApiResult> results_;

    std::uint64_t Submit(ApiJob job, bool priority);
    void ThreadMain();
};

}  // namespace clockin
