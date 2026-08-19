#include "app/WorkflowSession.hpp"

namespace clockin {

void WorkflowSession::ResetWorkflow() {
    pending_rfid_uid.clear();
    user_name.clear();
    user_department.clear();
    action.clear();
    request = WorkflowRequest::None;
    request_id = 0;
}

}  // namespace clockin
