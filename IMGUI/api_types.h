#ifndef API_TYPES_H
#define API_TYPES_H

#include <stdint.h>
#include <string>
#include <vector>

enum class ApiJobType {
    Health,
    Scan,
    Attendance,
    Signature
};

enum class ApiOutcome {
    Ok,
    Rejected,
    TransportFailed,
    Uncertain,
    Malformed,
    Cancelled
};

struct ScanResponse {
    bool success;
    std::string action;
    std::string message;
    std::string user_name;
    std::string user_department;

    ScanResponse() : success(false) {}
};

struct ApiJob {
    uint64_t id;
    ApiJobType type;
    std::string rfid_uid;
    std::string payload;

    ApiJob() : id(0), type(ApiJobType::Health) {}
};

struct ApiResult {
    uint64_t id;
    ApiJobType type;
    ApiOutcome outcome;
    long http_code;
    std::string message;
    ScanResponse scan;
    std::vector<std::string> attendance_dates;

    ApiResult()
        : id(0),
          type(ApiJobType::Health),
          outcome(ApiOutcome::TransportFailed),
          http_code(0) {}

    bool IsOk() const { return outcome == ApiOutcome::Ok; }
};

inline bool IsCurrentApiResult(uint64_t expected_id, const ApiResult& result) {
    return expected_id != 0 && result.id == expected_id;
}

#endif
