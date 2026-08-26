#pragma once

#include <cstdint>
#include <string>

namespace clockin {

enum class WorkflowRequest { None, Scan, Attendance, Signature };

struct WorkflowSession {
    std::string pending_rfid_uid;
    std::string user_name;
    std::string user_department;
    std::string action;
    WorkflowRequest request = WorkflowRequest::None;
    std::uint64_t request_id = 0;
    bool health_online = false;
    bool health_known = false;
    bool health_pending = false;
    bool health_refresh_requested = false;
    bool require_card_removal = false;
    std::string removal_uid;
    double next_health_check = 0.0;

    bool ApiBusy() const { return request_id != 0; }
    void ResetWorkflow();
};

}  // namespace clockin
