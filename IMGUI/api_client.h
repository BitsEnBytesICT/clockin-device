#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>

#include "api_types.h"
#include "tiny_json.h"

#define API_BASE_URL "https://management-acc.bitsenbytes.net"
#define API_DEVELOPMENT_KEY "test-api-key"
#define API_ATTENDANCE_PATH "/api/attendance_last_30"

class APIClient {
public:
    struct Config {
        std::string base_url;
        std::string api_key;
        bool mock_api;

        Config() : mock_api(false) {}
    };

    static Config ResolveConfig() {
        Config config;
        const char* configured_url = std::getenv("BITS_BYTES_API_BASE_URL");
        config.base_url = configured_url != NULL ? Trim(configured_url) : API_BASE_URL;
        while (!config.base_url.empty() && config.base_url[config.base_url.size() - 1] == '/') {
            config.base_url.erase(config.base_url.size() - 1);
        }
        if (config.base_url.empty()) config.base_url = API_BASE_URL;

        const char* configured_key = std::getenv("BITS_BYTES_API_KEY");
        if (configured_key != NULL) config.api_key = Trim(configured_key);
        if (config.api_key.empty()) {
            const char* configured_path = std::getenv("BITS_BYTES_API_KEY_FILE");
            const std::string path = configured_path != NULL
                ? Trim(configured_path)
                : "/etc/bitsenbytes/rfid-api-key";
            config.api_key = ReadFirstLine(path);
        }
        if (config.api_key.empty()) config.api_key = API_DEVELOPMENT_KEY;

#ifdef DESKTOP_SIM
        const char* mock_value = std::getenv("STM32_SIM_MOCK_API");
        config.mock_api = mock_value != NULL && std::string(mock_value) == "1";
#endif
        return config;
    }

    explicit APIClient(const Config& config)
        : config_(config), curl_(NULL) {}

    ~APIClient() {
        if (curl_ != NULL) curl_easy_cleanup(curl_);
    }

    ApiResult Execute(const ApiJob& job, const std::atomic<bool>& cancel) {
        if (config_.mock_api) return ExecuteMock(job, cancel);

        std::string url;
        std::string body;
        long timeout_seconds = 5;
        bool side_effect = false;

        switch (job.type) {
            case ApiJobType::Health:
                url = config_.base_url + "/api/health";
                timeout_seconds = 3;
                break;
            case ApiJobType::Scan:
                url = config_.base_url + "/api/scan";
                body = "{\"rfid_uid\":\"" + EscapeJson(CleanRFID(job.rfid_uid)) + "\"}";
                side_effect = true;
                break;
            case ApiJobType::Attendance:
                url = config_.base_url + API_ATTENDANCE_PATH;
                body = "{\"rfid_uid\":\"" + EscapeJson(CleanRFID(job.rfid_uid)) + "\"}";
                timeout_seconds = 8;
                break;
            case ApiJobType::Signature:
                url = config_.base_url + "/api/clock_in_with_signature";
                body = "{\"rfid_uid\":\"" + EscapeJson(CleanRFID(job.rfid_uid)) +
                       "\",\"signature\":\"" + EscapeJson(job.payload) + "\"}";
                timeout_seconds = 10;
                side_effect = true;
                break;
        }

        HttpResponse http = Perform(url, body, timeout_seconds, cancel);
        ApiResult result;
        result.id = job.id;
        result.type = job.type;
        result.http_code = http.status;

        if (http.curl_code == CURLE_ABORTED_BY_CALLBACK || cancel.load()) {
            result.outcome = ApiOutcome::Cancelled;
            result.message = "Verzoek geannuleerd";
            return result;
        }
        if (http.curl_code != CURLE_OK) {
            result.outcome = side_effect && http.request_size > 0
                ? ApiOutcome::Uncertain
                : ApiOutcome::TransportFailed;
            result.message = result.outcome == ApiOutcome::Uncertain
                ? "Resultaat onbekend"
                : "API-verbinding mislukt";
            printf("- API transport error for request %llu: %s\n",
                   static_cast<unsigned long long>(job.id),
                   curl_easy_strerror(http.curl_code));
            return result;
        }

        if (http.status != 200) {
            if (side_effect && http.status >= 500) {
                result.outcome = ApiOutcome::Uncertain;
                result.message = "Resultaat onbekend";
            } else if (http.status == 401 || http.status == 403 || http.status == 423) {
                result.outcome = ApiOutcome::Rejected;
                result.message = "API-autorisatie mislukt";
            } else if (http.status == 404 && job.type == ApiJobType::Scan) {
                result.outcome = ApiOutcome::Rejected;
                result.message = "Kaart niet geregistreerd";
            } else {
                result.outcome = ApiOutcome::Rejected;
                result.message = "Serverfout (HTTP " + std::to_string(http.status) + ")";
            }
            printf("- API %s request %llu with HTTP %ld\n",
                   result.outcome == ApiOutcome::Uncertain ? "left an uncertain" : "rejected",
                   static_cast<unsigned long long>(job.id), http.status);
            return result;
        }

        if (job.type == ApiJobType::Health || job.type == ApiJobType::Signature) {
            result.outcome = ApiOutcome::Ok;
            return result;
        }

        TinyJson document(std::move(http.body));
        TinyJson::ObjectRange root;
        if (!document.RootObject(root)) return MalformedResult(job, side_effect);

        if (job.type == ApiJobType::Attendance) {
            if (!document.GetStringArray(root, "dates", result.attendance_dates)) {
                return MalformedResult(job, false);
            }
            if (result.attendance_dates.size() > 64) return MalformedResult(job, false);
            for (size_t i = 0; i < result.attendance_dates.size(); ++i) {
                if (result.attendance_dates[i].size() > 64) return MalformedResult(job, false);
            }
            result.outcome = ApiOutcome::Ok;
            return result;
        }

        bool success = false;
        if (!document.GetBool(root, "success", success)) return MalformedResult(job, true);
        result.scan.success = success;
        document.GetString(root, "message", result.scan.message);
        if (!success) {
            result.outcome = ApiOutcome::Rejected;
            result.message = result.scan.message.empty() ? "Kaart geweigerd" : result.scan.message;
            return result;
        }
        if (!document.GetString(root, "action", result.scan.action) ||
            (result.scan.action != "clock_in" && result.scan.action != "clock_out")) {
            return MalformedResult(job, true);
        }
        TinyJson::ObjectRange user;
        if (document.GetObject(root, "user", user)) {
            document.GetString(user, "name", result.scan.user_name);
            document.GetString(user, "department", result.scan.user_department);
        }
        if (result.scan.message.size() > 512 || result.scan.user_name.size() > 256 ||
            result.scan.user_department.size() > 256) {
            return MalformedResult(job, true);
        }
        result.outcome = ApiOutcome::Ok;
        return result;
    }

    static std::string CleanRFID(std::string uid) {
        uid.erase(std::remove_if(uid.begin(), uid.end(), [](char c) {
            return std::isspace(static_cast<unsigned char>(c)) != 0;
        }), uid.end());
        return uid;
    }

private:
    static const size_t MAX_RESPONSE_BYTES = 64 * 1024;

    struct WriteContext {
        std::string body;
        bool overflow;
        WriteContext() : overflow(false) { body.reserve(2048); }
    };

    struct HttpResponse {
        CURLcode curl_code;
        long status;
        long request_size;
        std::string body;

        HttpResponse() : curl_code(CURLE_FAILED_INIT), status(0), request_size(0) {}
    };

    Config config_;
    CURL* curl_;

    static std::string Trim(const std::string& value) {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static std::string ReadFirstLine(const std::string& path) {
        if (path.empty()) return "";
        std::ifstream input(path.c_str());
        if (!input.is_open()) return "";
        std::string line;
        std::getline(input, line);
        return Trim(line);
    }

    static std::string EscapeJson(const std::string& input) {
        std::ostringstream escaped;
        for (size_t i = 0; i < input.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(input[i]);
            switch (c) {
                case '"': escaped << "\\\""; break;
                case '\\': escaped << "\\\\"; break;
                case '\b': escaped << "\\b"; break;
                case '\f': escaped << "\\f"; break;
                case '\n': escaped << "\\n"; break;
                case '\r': escaped << "\\r"; break;
                case '\t': escaped << "\\t"; break;
                default:
                    if (c < 0x20) {
                        char encoded[8];
                        snprintf(encoded, sizeof(encoded), "\\u%04x", static_cast<unsigned int>(c));
                        escaped << encoded;
                    } else {
                        escaped << static_cast<char>(c);
                    }
                    break;
            }
        }
        return escaped.str();
    }

    static size_t WriteCallback(void* contents, size_t size, size_t count, void* user_data) {
        WriteContext* context = static_cast<WriteContext*>(user_data);
        const size_t bytes = size * count;
        if (context->body.size() + bytes > MAX_RESPONSE_BYTES) {
            context->overflow = true;
            return 0;
        }
        context->body.append(static_cast<const char*>(contents), bytes);
        return bytes;
    }

    static int ProgressCallback(void* user_data,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t) {
        const std::atomic<bool>* cancel = static_cast<const std::atomic<bool>*>(user_data);
        return cancel->load() ? 1 : 0;
    }

    HttpResponse Perform(const std::string& url,
                         const std::string& body,
                         long timeout_seconds,
                         const std::atomic<bool>& cancel) {
        HttpResponse response;
        if (curl_ == NULL) curl_ = curl_easy_init();
        if (curl_ == NULL) return response;

        curl_easy_reset(curl_); // Retains libcurl's connection and TLS caches.
        WriteContext write_context;
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!config_.api_key.empty()) {
            const std::string authorization = "Authorization: " + config_.api_key;
            headers = curl_slist_append(headers, authorization.c_str());
        }

        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &write_context);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, "bits-bytes-stm32-rfid/2.0");
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 3L);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds);
        curl_easy_setopt(curl_, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
        curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, &cancel);
        if (body.empty()) {
            curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
        } else {
            curl_easy_setopt(curl_, CURLOPT_POST, 1L);
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.data());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
        }

        response.curl_code = curl_easy_perform(curl_);
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status);
        curl_easy_getinfo(curl_, CURLINFO_REQUEST_SIZE, &response.request_size);
        response.body.swap(write_context.body);
        curl_slist_free_all(headers);
        return response;
    }

    static ApiResult MalformedResult(const ApiJob& job, bool side_effect) {
        ApiResult result;
        result.id = job.id;
        result.type = job.type;
        result.http_code = 200;
        result.outcome = side_effect ? ApiOutcome::Uncertain : ApiOutcome::Malformed;
        result.message = side_effect ? "Resultaat onbekend" : "Ongeldig antwoord van server";
        return result;
    }

    ApiResult ExecuteMock(const ApiJob& job, const std::atomic<bool>& cancel) const {
        ApiResult result;
        result.id = job.id;
        result.type = job.type;
        result.http_code = 200;

        const char* delay_value = std::getenv("STM32_SIM_API_DELAY_MS");
        const int delay_ms = delay_value != NULL ? std::max(0, atoi(delay_value)) : 0;
        for (int waited = 0; waited < delay_ms; waited += 5) {
            if (cancel.load()) {
                result.outcome = ApiOutcome::Cancelled;
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const char* fault_value = std::getenv("STM32_SIM_API_FAILURE");
        const std::string fault = fault_value != NULL ? fault_value : "";
        if (fault == "uncertain" && (job.type == ApiJobType::Scan || job.type == ApiJobType::Signature)) {
            result.outcome = ApiOutcome::Uncertain;
            result.message = "Resultaat onbekend";
            return result;
        }
        if (fault == "offline") {
            result.outcome = ApiOutcome::TransportFailed;
            result.http_code = 0;
            result.message = "API-verbinding mislukt";
            return result;
        }
        if (fault == "malformed") {
            result.outcome = job.type == ApiJobType::Scan ? ApiOutcome::Uncertain : ApiOutcome::Malformed;
            result.message = result.outcome == ApiOutcome::Uncertain
                ? "Resultaat onbekend"
                : "Ongeldig antwoord van server";
            return result;
        }
        if (fault == "http500") {
            result.outcome = (job.type == ApiJobType::Scan || job.type == ApiJobType::Signature)
                ? ApiOutcome::Uncertain
                : ApiOutcome::Rejected;
            result.http_code = 500;
            result.message = result.outcome == ApiOutcome::Uncertain
                ? "Resultaat onbekend"
                : "Serverfout (HTTP 500)";
            return result;
        }
        if (fault == "auth") {
            result.outcome = ApiOutcome::Rejected;
            result.http_code = 401;
            result.message = "API-autorisatie mislukt";
            return result;
        }

        result.outcome = ApiOutcome::Ok;
        if (job.type == ApiJobType::Scan) {
            const std::string uid = CleanRFID(job.rfid_uid);
            if (uid.empty() || uid == "SIM_UNKNOWN") {
                result.outcome = ApiOutcome::Rejected;
                result.message = "Kaart niet geregistreerd";
                return result;
            }
            result.scan.success = true;
            result.scan.action = uid == "SIM_CLOCK_OUT" ? "clock_out" : "clock_in";
            result.scan.message = result.scan.action == "clock_out" ? "Uitgeklokt" : "Klaar voor handtekening";
            result.scan.user_name = result.scan.action == "clock_out" ? "Simone Tester" : "Derk Simulator";
            result.scan.user_department = "Desktop simulation";
        } else if (job.type == ApiJobType::Attendance) {
            const char* dates[] = {
                "2026-08-12", "2026-08-11", "2026-08-08", "2026-08-07",
                "2026-08-06", "2026-08-05", "2026-08-04", "2026-08-01",
                "2026-07-31", "2026-07-30", "2026-07-29", "2026-07-28"
            };
            result.attendance_dates.assign(dates, dates + sizeof(dates) / sizeof(dates[0]));
        }
        return result;
    }
};

#endif
