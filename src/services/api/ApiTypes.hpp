#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace clockin {

enum class ApiJobType { Health, Scan, Attendance, Signature };

enum class ApiOutcome {
    Ok,
    Rejected,
    TransportFailed,
    Uncertain,
    Malformed,
    Cancelled
};

struct ScanResponse {
    bool success = false;
    std::string action;
    std::string message;
    std::string user_name;
    std::string user_department;
};

struct ApiJob {
    std::uint64_t id = 0;
    ApiJobType type = ApiJobType::Health;
    std::string rfid_uid;
    std::string payload;
};

struct ApiResult {
    std::uint64_t id = 0;
    ApiJobType type = ApiJobType::Health;
    ApiOutcome outcome = ApiOutcome::TransportFailed;
    long http_code = 0;
    std::string message;
    ScanResponse scan;
    std::vector<std::string> attendance_dates;

    bool IsOk() const { return outcome == ApiOutcome::Ok; }
};

inline bool IsCurrentApiResult(std::uint64_t expected_id, const ApiResult& result) {
    return expected_id != 0 && result.id == expected_id;
}

}  // namespace clockin
