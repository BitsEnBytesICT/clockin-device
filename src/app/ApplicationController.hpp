#pragma once

#include "app/AppEvent.hpp"
#include "app/AppConfiguration.hpp"
#include "app/InputRouter.hpp"
#include "app/Navigation.hpp"
#include "app/WorkflowCoordinator.hpp"
#include "app/WorkflowView.hpp"
#include "pages/AdminPage.hpp"
#include "pages/AdminPinPage.hpp"
#include "pages/AttendancePage.hpp"
#include "pages/ErrorPage.hpp"
#include "pages/ProcessingPage.hpp"
#include "pages/SignaturePage.hpp"
#include "pages/SuccessPage.hpp"
#include "pages/WaitingPage.hpp"
#include "services/display/BacklightController.hpp"
#include "services/display/PerformanceMetrics.hpp"
#include "services/config/AdminPinStore.hpp"
#include "services/input/TouchInput.hpp"
#include "services/network/NetworkTypes.hpp"
#include "ui/components/ButtonFeedback.hpp"
#include "ui/components/PageDrawing.hpp"

namespace clockin {

class IApiService;
class IHardwareService;
class INetworkService;

class ApplicationController final : public IWorkflowView {
public:
    ApplicationController(AppConfiguration configuration,
                          IApiService& api,
                          IHardwareService& hardware,
                          INetworkService& network);

    void Start(double now_seconds);
    void Stop();
    void Update(double delta, double now_seconds);
    void Render(double now_seconds);
    void SetFonts(ImFont* regular, ImFont* semibold, bool icons_available);
    void SetDisplayRefresh(int hertz) { display_refresh_hz_ = hertz; }
    void RecordFrame(double now_seconds,
                     double frame_seconds,
                     double update_ms,
                     double render_ms,
                     double swap_ms,
                     int vertices,
                     int indices,
                     int draw_calls);

    PageId CurrentPage() const { return navigation_.Current(); }

    void ShowProcessing(const std::string& message, double now_seconds) override;
    void ShowAttendance(const std::string& user_name,
                        const std::vector<std::string>& dates,
                        const std::string& warning,
                        double now_seconds) override;
    void ShowSuccess(const std::string& user_name,
                     const std::string& action,
                     double now_seconds) override;
    void ShowError(const std::string& message,
                   const std::string& detail,
                   double now_seconds) override;
    void ShowAdminCard(const std::string& uid) override;
    void ShowWaiting(double now_seconds) override;

private:
    AppConfiguration configuration_;
    IHardwareService& hardware_;
    INetworkService& network_;
    WorkflowCoordinator workflow_;
    AdminPinStore pin_store_;
    TouchInput touch_;
    BacklightController backlight_;
    PerformanceMetrics performance_;
    Navigation navigation_;
    InputRouter input_router_;
    ButtonFeedback button_feedback_;
    PageDrawing drawing_;
    WaitingPage waiting_page_;
    ProcessingPage processing_page_;
    AttendancePage attendance_page_;
    SignaturePage signature_page_;
    SuccessPage success_page_;
    ErrorPage error_page_;
    AdminPinPage admin_pin_page_;
    AdminPage admin_page_;
    NetworkSnapshot network_snapshot_;
    double admin_last_activity_ = 0.0;
    int display_refresh_hz_ = 0;

    Page& ActivePage();
    const Page& ActivePage() const;
    void ChangePage(PageId page, double now_seconds);
    void Activate(const AppEvent& event);
    void HandleAdminAction(AdminAction action, double now_seconds);
    void RenderPerformanceOverlay() const;
};

}  // namespace clockin
