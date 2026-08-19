#include "services/api/ApiClient.hpp"

#include "domain/RfidUid.hpp"
#include "services/api/TinyJson.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <utility>

namespace clockin {
namespace {

constexpr const char* kAttendancePath = "/api/attendance_last_30";

}  // namespace

ApiClient::ApiClient(std::string base_url, std::string api_key)
    : base_url_(std::move(base_url)), api_key_(std::move(api_key)) {}

ApiClient::~ApiClient() {
    if (curl_ != nullptr) curl_easy_cleanup(curl_);
}

ApiResult ApiClient::Execute(const ApiJob& job, const std::atomic<bool>& cancel) {
    std::string url;
    std::string body;
    long timeout_seconds = 5;
    bool side_effect = false;
    switch (job.type) {
        case ApiJobType::Health:
            url = base_url_ + "/api/health";
            timeout_seconds = 3;
            break;
        case ApiJobType::Scan:
            url = base_url_ + "/api/scan";
            body = "{\"rfid_uid\":\"" + EscapeJson(NormalizeRfidUid(job.rfid_uid)) + "\"}";
            side_effect = true;
            break;
        case ApiJobType::Attendance:
            url = base_url_ + kAttendancePath;
            body = "{\"rfid_uid\":\"" + EscapeJson(NormalizeRfidUid(job.rfid_uid)) + "\"}";
            timeout_seconds = 8;
            break;
        case ApiJobType::Signature:
            url = base_url_ + "/api/clock_in_with_signature";
            body = "{\"rfid_uid\":\"" + EscapeJson(NormalizeRfidUid(job.rfid_uid)) +
                   "\",\"signature\":\"" + EscapeJson(job.payload) + "\"}";
            timeout_seconds = 10;
            side_effect = true;
            break;
    }

    const HttpResponse http = Perform(url, body, timeout_seconds, cancel);
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
        std::fprintf(stderr, "API request %llu failed: %s\n",
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
        return result;
    }
    if (job.type == ApiJobType::Health || job.type == ApiJobType::Signature) {
        result.outcome = ApiOutcome::Ok;
        return result;
    }

    TinyJson document(http.body);
    TinyJson::ObjectRange root;
    if (!document.RootObject(root)) return MalformedResult(job, side_effect);
    if (job.type == ApiJobType::Attendance) {
        if (!document.GetStringArray(root, "dates", result.attendance_dates) ||
            result.attendance_dates.size() > 64) {
            return MalformedResult(job, false);
        }
        for (const std::string& date : result.attendance_dates) {
            if (date.size() > 64) return MalformedResult(job, false);
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

std::string ApiClient::EscapeJson(const std::string& input) {
    std::ostringstream escaped;
    for (unsigned char character : input) {
        switch (character) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (character < 0x20) {
                    char encoded[8];
                    std::snprintf(encoded, sizeof(encoded), "\\u%04x", character);
                    escaped << encoded;
                } else {
                    escaped << static_cast<char>(character);
                }
        }
    }
    return escaped.str();
}

std::size_t ApiClient::WriteCallback(void* contents,
                                     std::size_t size,
                                     std::size_t count,
                                     void* user_data) {
    auto* context = static_cast<WriteContext*>(user_data);
    const std::size_t bytes = size * count;
    if (context->body.size() + bytes > kMaxResponseBytes) {
        context->overflow = true;
        return 0;
    }
    context->body.append(static_cast<const char*>(contents), bytes);
    return bytes;
}

int ApiClient::ProgressCallback(void* user_data,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t) {
    return static_cast<const std::atomic<bool>*>(user_data)->load() ? 1 : 0;
}

ApiClient::HttpResponse ApiClient::Perform(const std::string& url,
                                           const std::string& body,
                                           long timeout_seconds,
                                           const std::atomic<bool>& cancel) {
    HttpResponse response;
    if (url.empty()) return response;
    if (curl_ == nullptr) curl_ = curl_easy_init();
    if (curl_ == nullptr) return response;

    curl_easy_reset(curl_);
    WriteContext write_context;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key_.empty()) {
        const std::string authorization = "Authorization: " + api_key_;
        headers = curl_slist_append(headers, authorization.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &write_context);
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "bits-bytes-stm32-rfid/3.0");
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
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(body.size()));
    }
    response.curl_code = curl_easy_perform(curl_);
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status);
    curl_easy_getinfo(curl_, CURLINFO_REQUEST_SIZE, &response.request_size);
    response.body.swap(write_context.body);
    curl_slist_free_all(headers);
    return response;
}

ApiResult ApiClient::MalformedResult(const ApiJob& job, bool side_effect) {
    ApiResult result;
    result.id = job.id;
    result.type = job.type;
    result.http_code = 200;
    result.outcome = side_effect ? ApiOutcome::Uncertain : ApiOutcome::Malformed;
    result.message = side_effect ? "Resultaat onbekend" : "Ongeldig antwoord van server";
    return result;
}

}  // namespace clockin
