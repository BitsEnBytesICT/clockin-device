#pragma once

#include <string>
#include <vector>

namespace clockin {

class IWorkflowView {
public:
    virtual ~IWorkflowView() = default;
    virtual void ShowProcessing(const std::string& message, double now_seconds) = 0;
    virtual void ShowAttendance(const std::string& user_name,
                                const std::vector<std::string>& dates,
                                const std::string& warning,
                                double now_seconds) = 0;
    virtual void ShowSuccess(const std::string& user_name,
                             const std::string& action,
                             double now_seconds) = 0;
    virtual void ShowError(const std::string& message,
                           const std::string& detail,
                           double now_seconds) = 0;
    virtual void ShowAdminCard(const std::string& uid) = 0;
    virtual void ShowWaiting(double now_seconds) = 0;
};

}  // namespace clockin
