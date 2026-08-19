#pragma once

#include "app/WorkflowView.hpp"
#include "pages/Page.hpp"

namespace clockin::test {

class RecordingWorkflowView final : public IWorkflowView {
public:
    void ShowProcessing(const std::string& value, double) override {
        page = PageId::Processing;
        message = value;
    }
    void ShowAttendance(const std::string& name,
                        const std::vector<std::string>& values,
                        const std::string& value,
                        double) override {
        page = PageId::Attendance;
        user_name = name;
        dates = values;
        warning = value;
    }
    void ShowSuccess(const std::string& name,
                     const std::string& value,
                     double) override {
        page = PageId::Success;
        user_name = name;
        action = value;
    }
    void ShowError(const std::string& value,
                   const std::string& detail_value,
                   double) override {
        page = PageId::Error;
        message = value;
        detail = detail_value;
    }
    void ShowAdminCard(const std::string& uid) override { admin_uid = uid; }
    void ShowWaiting(double) override { page = PageId::Waiting; }

    PageId page = PageId::Waiting;
    std::string message;
    std::string detail;
    std::string warning;
    std::string user_name;
    std::string action;
    std::string admin_uid;
    std::vector<std::string> dates;
};

}  // namespace clockin::test
