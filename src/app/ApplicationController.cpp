#include "app/ApplicationController.hpp"

#include "services/api/IApiService.hpp"
#include "services/hardware/IHardwareService.hpp"
#include "services/network/INetworkService.hpp"
#include "ui/components/PageTransition.hpp"
#include "ui/components/PinKeypad.hpp"

#include <cstdio>
#include <utility>

namespace clockin {
namespace {

constexpr const char* kBrightnessPath = "/sys/class/backlight/5a000000.dsi.0/brightness";
constexpr const char* kMaximumBrightnessPath =
    "/sys/class/backlight/5a000000.dsi.0/max_brightness";
constexpr double kMessageDurationSeconds = 3.0;
constexpr double kAdminTimeoutSeconds = 120.0;

bool IsNetworkOperationState(NetworkState state) {
    return state == NetworkState::Scanning ||
           state == NetworkState::Associating ||
           state == NetworkState::AcquiringAddress ||
           state == NetworkState::RollingBack;
}

}  // namespace

ApplicationController::ApplicationController(AppConfiguration configuration,
                                             IApiService& api,
                                             IHardwareService& hardware,
                                             INetworkService& network)
    : configuration_(std::move(configuration)),
      hardware_(hardware),
      network_(network),
      workflow_(api, hardware),
      touch_(configuration_.touch_device),
      backlight_(kBrightnessPath, kMaximumBrightnessPath),
      performance_(configuration_.performance_overlay) {}

void ApplicationController::Start(double now_seconds) {
    backlight_.Initialize();
    workflow_.Start(now_seconds);
    if (!network_.Start()) {
        std::fprintf(stderr, "Network worker could not start; admin UI remains available\n");
    }
    waiting_page_.SetApiStatus(workflow_.Session().health_known,
                               workflow_.Session().health_online);
}

void ApplicationController::Stop() {
    network_.Stop();
    workflow_.Stop();
}

void ApplicationController::Update(double delta, double now_seconds) {
    touch_.Update(now_seconds);
    workflow_.Update(navigation_.Current(), now_seconds, *this);
    NetworkSnapshot network_update;
    while (network_.TryPop(network_update)) {
        const bool initialized = network_snapshot_.revision != 0;
        const bool connection_generation_changed =
            network_update.connection_generation !=
            network_snapshot_.connection_generation;
        const bool operation_failed = initialized &&
            network_update.state == NetworkState::Error &&
            network_snapshot_.state != NetworkState::Error &&
            IsNetworkOperationState(network_snapshot_.state);
        network_snapshot_ = std::move(network_update);
        admin_page_.SetNetworkSnapshot(network_snapshot_);
        if (connection_generation_changed) {
            workflow_.RequestHealth(now_seconds);
            if (initialized) hardware_.PlayFeedback(FeedbackCue::Success);
        } else if (operation_failed) {
            hardware_.PlayFeedback(FeedbackCue::Error);
        }
    }
    admin_page_.SetApiStatus(workflow_.Session().health_known,
                             workflow_.Session().health_online);
    Page& active = ActivePage();
    const bool touch_activity = input_router_.Process(
        touch_.Events(), active, signature_page_, button_feedback_,
        now_seconds,
        [this](const AppEvent& event) { Activate(event); });
    const bool hardware_activity = workflow_.ConsumeActivity();
    waiting_page_.SetApiStatus(workflow_.Session().health_known,
                               workflow_.Session().health_online);
    if ((navigation_.Current() == PageId::Success ||
         navigation_.Current() == PageId::Error) &&
        now_seconds - navigation_.EnteredAt() >= kMessageDurationSeconds) {
        workflow_.ReturnToWaiting(now_seconds, *this);
    }
    if (navigation_.Current() == PageId::Admin) {
        if (touch_activity) admin_last_activity_ = now_seconds;
        if (now_seconds - admin_last_activity_ >= kAdminTimeoutSeconds) {
            admin_page_.ClearSecrets();
            workflow_.ReturnToWaiting(now_seconds, *this);
        }
    }
    backlight_.Update(touch_activity || hardware_activity, delta, now_seconds);
}

void ApplicationController::Render(double now_seconds) {
    drawing_.BeginFrame(&button_feedback_, now_seconds);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const int first_page_vertex = draw->VtxBuffer.Size;
    ActivePage().Render(*draw, drawing_);
    ApplyPageTransition(
        *draw, first_page_vertex,
        EvaluatePageTransition(now_seconds - navigation_.EnteredAt()));
    RenderPerformanceOverlay();
}

void ApplicationController::SetFonts(ImFont* regular,
                                     ImFont* semibold,
                                     bool icons_available) {
    drawing_.SetFonts(regular, semibold, icons_available);
}

void ApplicationController::RecordFrame(double now_seconds,
                                        double frame_seconds,
                                        double update_ms,
                                        double render_ms,
                                        double swap_ms,
                                        int vertices,
                                        int indices,
                                        int draw_calls) {
    performance_.Add(now_seconds, frame_seconds, update_ms, render_ms, swap_ms,
                     touch_.LastSampleLatencyMs(now_seconds), vertices, indices, draw_calls);
}

void ApplicationController::ShowProcessing(const std::string& message, double now_seconds) {
    processing_page_.SetMessage(message);
    ChangePage(PageId::Processing, now_seconds);
}

void ApplicationController::ShowAttendance(const std::string& user_name,
                                           const std::vector<std::string>& dates,
                                           const std::string& warning,
                                           double now_seconds) {
    attendance_page_.SetData(user_name, dates, warning);
    ChangePage(PageId::Attendance, now_seconds);
}

void ApplicationController::ShowSuccess(const std::string& user_name,
                                        const std::string& action,
                                        double now_seconds) {
    success_page_.SetResult(user_name, action, now_seconds);
    ChangePage(PageId::Success, now_seconds);
}

void ApplicationController::ShowError(const std::string& message,
                                      const std::string& detail,
                                      double now_seconds) {
    error_page_.SetError(message, detail, now_seconds);
    ChangePage(PageId::Error, now_seconds);
}

void ApplicationController::ShowAdminCard(const std::string& uid) {
    admin_page_.SetUid(uid);
}

void ApplicationController::ShowWaiting(double now_seconds) {
    attendance_page_.Reset();
    signature_page_.Clear();
    admin_pin_page_.Reset();
    admin_page_.ClearSecrets();
    ChangePage(PageId::Waiting, now_seconds);
}

Page& ApplicationController::ActivePage() {
    switch (navigation_.Current()) {
        case PageId::Waiting: return waiting_page_;
        case PageId::Processing: return processing_page_;
        case PageId::Attendance: return attendance_page_;
        case PageId::Signature: return signature_page_;
        case PageId::Success: return success_page_;
        case PageId::Error: return error_page_;
        case PageId::AdminPin: return admin_pin_page_;
        case PageId::Admin: return admin_page_;
    }
    return waiting_page_;
}

const Page& ApplicationController::ActivePage() const {
    return const_cast<ApplicationController*>(this)->ActivePage();
}

void ApplicationController::ChangePage(PageId page, double now_seconds) {
    if (navigation_.Current() == PageId::Admin && page != PageId::Admin) {
        admin_page_.ClearSecrets();
    }
    if (!navigation_.Change(page, now_seconds)) return;
    if (page == PageId::Admin) admin_last_activity_ = now_seconds;
    input_router_.Reset();
    button_feedback_.Clear();
    touch_.SuppressUntilRelease();
}

void ApplicationController::Activate(const AppEvent& event) {
    const UiControl control = event.control;
    const double now_seconds = event.occurred_at;
    if (navigation_.Current() == PageId::Admin) {
        admin_last_activity_ = now_seconds;
        HandleAdminAction(admin_page_.Activate(event), now_seconds);
        return;
    }
    switch (control) {
        case UiControl::Admin:
            if (!configuration_.AdminEnabled()) {
                hardware_.PlayFeedback(FeedbackCue::Error);
                ShowError("Beheerderspincode niet geconfigureerd", {}, now_seconds);
            } else {
                admin_pin_page_.Reset();
                ChangePage(PageId::AdminPin, now_seconds);
            }
            break;
        case UiControl::AttendanceBack:
        case UiControl::SignatureCancel:
        case UiControl::AdminBack:
            workflow_.ReturnToWaiting(now_seconds, *this);
            break;
        case UiControl::AttendanceConfirm:
            hardware_.PlayFeedback(FeedbackCue::Tap);
            signature_page_.Start(workflow_.Session().user_name);
            ChangePage(PageId::Signature, now_seconds);
            break;
        case UiControl::SignatureClear:
            signature_page_.Clear();
            break;
        case UiControl::SignatureSubmit: {
            std::string svg;
            if (signature_page_.BuildPayload(svg) &&
                workflow_.SubmitSignature(svg, now_seconds, *this)) {
                hardware_.PlayFeedback(FeedbackCue::Tap);
            }
            break;
        }
        default:
            if (PinKeypad::Digit(control) >= 0 || control == UiControl::KeyBackspace) {
                admin_pin_page_.ApplyKey(control, now_seconds);
                if (admin_pin_page_.Value().size() == configuration_.admin_pin.size()) {
                    if (admin_pin_page_.Value() == configuration_.admin_pin) {
                        admin_page_.Reset(configuration_.admin_pin_persistable,
                                          now_seconds);
                        admin_page_.SetNetworkSnapshot(network_snapshot_);
                        admin_page_.SetApiUrl(configuration_.api_base_url);
                        admin_page_.SetApiStatus(workflow_.Session().health_known,
                                                 workflow_.Session().health_online);
                        ChangePage(PageId::Admin, now_seconds);
                    } else {
                        admin_pin_page_.Reset();
                        hardware_.PlayFeedback(FeedbackCue::Error);
                        ShowError("Pincode onjuist", {}, now_seconds);
                    }
                }
            }
            break;
    }
}

void ApplicationController::RenderPerformanceOverlay() const {
    if (!performance_.Enabled()) return;
    const PerformanceSnapshot& metrics = performance_.Snapshot();
    char touch_latency[48];
    if (metrics.touch_latency_ms < 0.0) std::snprintf(touch_latency, sizeof(touch_latency), "n/a");
    else std::snprintf(touch_latency, sizeof(touch_latency), "%.2f ms", metrics.touch_latency_ms);
    char lines[768];
    std::snprintf(
        lines, sizeof(lines),
        "Display: %d Hz (VSync)   FPS: %.1f\n"
        "Frame ms p50/p95/p99: %.2f / %.2f / %.2f\n"
        "Update/render/swap p95: %.2f / %.2f / %.2f ms\n"
        "Touch latency: %s   Signature raw/kept: %zu / %zu\n"
        "Draw: %d calls, %d vertices, %d indices\n"
        "Touch/RFID reconnects: %u / %u   API: %s",
        display_refresh_hz_, metrics.fps, metrics.frame_p50_ms, metrics.frame_p95_ms,
        metrics.frame_p99_ms, metrics.update_p95_ms, metrics.render_p95_ms,
        metrics.swap_p95_ms, touch_latency, signature_page_.RawSampleCount(),
        signature_page_.PointCount(), metrics.draw_calls, metrics.vertices, metrics.indices,
        touch_.ReconnectCount(), hardware_.ReconnectCount(),
        workflow_.Session().ApiBusy() ? "busy"
                                      : (workflow_.Session().health_online ? "online" : "offline"));
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled({8, 8}, {460, 155}, IM_COL32(15, 15, 15, 225), 5.0f);
    draw->AddText({16, 15}, IM_COL32(255, 255, 255, 255), lines);
}

}  // namespace clockin
