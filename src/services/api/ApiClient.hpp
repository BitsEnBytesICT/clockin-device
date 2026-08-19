#pragma once

#include "services/api/ApiTypes.hpp"

#include <atomic>
#include <cstddef>
#include <curl/curl.h>
#include <string>

namespace clockin {

class ApiClient {
public:
    ApiClient(std::string base_url, std::string api_key);
    ~ApiClient();

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    ApiResult Execute(const ApiJob& job, const std::atomic<bool>& cancel);

private:
    static constexpr std::size_t kMaxResponseBytes = 64 * 1024;

    struct WriteContext {
        std::string body;
        bool overflow = false;
        WriteContext() { body.reserve(2048); }
    };

    struct HttpResponse {
        CURLcode curl_code = CURLE_FAILED_INIT;
        long status = 0;
        long request_size = 0;
        std::string body;
    };

    std::string base_url_;
    std::string api_key_;
    CURL* curl_ = nullptr;

    static std::string EscapeJson(const std::string& input);
    static std::size_t WriteCallback(void* contents,
                                     std::size_t size,
                                     std::size_t count,
                                     void* user_data);
    static int ProgressCallback(void* user_data,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t);
    HttpResponse Perform(const std::string& url,
                         const std::string& body,
                         long timeout_seconds,
                         const std::atomic<bool>& cancel);
    static ApiResult MalformedResult(const ApiJob& job, bool side_effect);
};

}  // namespace clockin
