#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <curl/curl.h>
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "api_worker.h"
#include "card_presence.h"
#include "performance_metrics.h"
#include "rfid_reader.h"
#include "signature_pad.h"
#include "touch_handler.h"
#include "ui_assets.h"
#include "ui_feedback.h"

#define KEYPAD_BUTTON_W 120
#define KEYPAD_BUTTON_H 80
#define KEYPAD_START_X 205
#define KEYPAD_START_Y 190
#define KEYPAD_SPACING_X 115
#define KEYPAD_SPACING_Y 90

#define BACKLIGHT_BRIGHTNESS_PATH "/sys/class/backlight/5a000000.dsi.0/brightness"
#define BACKLIGHT_MAX_PATH "/sys/class/backlight/5a000000.dsi.0/max_brightness"
#define BACKLIGHT_FULL_DEFAULT 240
#define BACKLIGHT_DIM_VALUE 120
#define BACKLIGHT_DIM_SECONDS 30.0
#define BACKLIGHT_OFF_SECONDS 60.0
#define BACKLIGHT_TRANSITION_SECONDS 0.5

#include "ui_renderer.h"

enum AppState {
    STATE_WAITING_CARD,
    STATE_PROCESSING,
    STATE_ATTENDANCE,
    STATE_SIGNATURE,
    STATE_SUCCESS,
    STATE_ERROR,
    STATE_ADMIN_PASSWORD,
    STATE_ADMIN
};

enum class WorkflowRequest {
    None,
    Scan,
    Attendance,
    Signature
};

typedef UIControl HitTarget;

struct GestureState {
    bool active;
    bool moved;
    HitTarget target;
    ImVec2 start;
    ImVec2 last;

    GestureState()
        : active(false), moved(false), target(HitTarget::None), start(0.0f, 0.0f), last(0.0f, 0.0f) {}

    void Clear() {
        active = false;
        moved = false;
        target = HitTarget::None;
    }
};

struct AppContext {
    AppState current_state;
    RFIDReader rfid_reader;
    TouchHandler touch_handler;
    APIWorker api_worker;
    UIRenderer ui_renderer;
    CardPresenceTracker card_presence;
    SignaturePad signature;
    PerformanceMetrics performance;
    GestureState gesture;
    UIButtonFeedback button_feedback;

    std::string pending_rfid_uid;
    std::string user_name;
    std::string user_department;
    std::string action;
    std::string message;
    std::string error_detail;
    std::string signature_warning;
    std::string admin_password_buffer;
    std::string admin_displayed_rfid;
    std::vector<std::string> attendance_dates;
    std::string attendance_warning;

    WorkflowRequest request_kind;
    uint64_t request_id;
    bool health_online;
    bool health_known;
    bool require_card_removal;
    std::string removal_uid;
    std::string processing_message;
    double next_health_check;

    double state_started;
    double inactivity_seconds;
    bool activity_detected;
    float attendance_scroll_offset;
    bool attendance_dragging;
    int admin_last_digit;
    double admin_last_digit_time;
    int backlight_max;
    int backlight_current;
    double backlight_current_f;
    int backlight_target;
    double backlight_transition_start;
    double backlight_transition_progress;
    double backlight_last_write;
    int display_refresh_hz;

    AppContext()
        : current_state(STATE_WAITING_CARD),
          card_presence(1.5),
          request_kind(WorkflowRequest::None),
          request_id(0),
          health_online(false),
          health_known(false),
          require_card_removal(false),
          next_health_check(0.0),
          state_started(0.0),
          inactivity_seconds(0.0),
          activity_detected(false),
          attendance_scroll_offset(0.0f),
          attendance_dragging(false),
          admin_last_digit(-1),
          admin_last_digit_time(0.0),
          backlight_max(BACKLIGHT_FULL_DEFAULT),
          backlight_current(-1),
          backlight_current_f(-1.0),
          backlight_target(-1),
          backlight_transition_start(0.0),
          backlight_transition_progress(1.0),
          backlight_last_write(-1.0),
          display_refresh_hz(0) {
        attendance_dates.reserve(32);
    }

    bool ApiBusy() const { return request_id != 0; }

    void ChangeState(AppState next, double now_seconds) {
        if (current_state == next) return;
        current_state = next;
        state_started = now_seconds;
        gesture.Clear();
        button_feedback.Clear();
        touch_handler.SuppressUntilRelease();
    }

    void ResetWorkflow() {
        pending_rfid_uid.clear();
        user_name.clear();
        user_department.clear();
        action.clear();
        message.clear();
        error_detail.clear();
        signature_warning.clear();
        attendance_dates.clear();
        attendance_warning.clear();
        attendance_scroll_offset = 0.0f;
        attendance_dragging = false;
        signature.Clear();
        request_kind = WorkflowRequest::None;
        request_id = 0;
        processing_message.clear();
        admin_password_buffer.clear();
        admin_last_digit = -1;
        gesture.Clear();
        button_feedback.Clear();
    }
};

static const std::string ADMIN_PASSWORD = "1111";
static const double MESSAGE_DURATION_SECONDS = 3.0;
static const float CLICK_MOVEMENT_LIMIT = 10.0f;

static double SteadySeconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

static bool PointInRect(const ImVec2& point, float x, float y, float width, float height) {
    return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
}

static float Distance(const ImVec2& a, const ImVec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static HitTarget TargetAt(AppState state, const ImVec2& position) {
    if (state == STATE_WAITING_CARD && PointInRect(position, 650, 10, 140, 60)) return HitTarget::Admin;
    if (state == STATE_ATTENDANCE) {
        if (PointInRect(position, 650, 10, 140, 60)) return HitTarget::AttendanceBack;
        if (PointInRect(position, 650, 400, 140, 60)) return HitTarget::AttendanceConfirm;
    }
    if (state == STATE_SIGNATURE) {
        if (PointInRect(position, 650, 10, 140, 60)) return HitTarget::SignatureClear;
        if (PointInRect(position, 650, 205, 140, 60)) return HitTarget::SignatureCancel;
        if (PointInRect(position, 650, 400, 140, 60)) return HitTarget::SignatureSubmit;
    }
    if (state == STATE_ADMIN_PASSWORD || state == STATE_ADMIN) {
        if (PointInRect(position, 650, 10, 140, 60)) return HitTarget::AdminBack;
    }
    if (state == STATE_ADMIN_PASSWORD) {
        for (int digit = 1; digit <= 9; ++digit) {
            const int row = (digit - 1) / 3;
            const int column = (digit - 1) % 3;
            const float x = KEYPAD_START_X + column * KEYPAD_SPACING_X;
            const float y = KEYPAD_START_Y + row * KEYPAD_SPACING_Y;
            if (PointInRect(position, x, y, KEYPAD_BUTTON_W, KEYPAD_BUTTON_H)) {
                return static_cast<HitTarget>(static_cast<int>(HitTarget::Key1) + digit - 1);
            }
        }
    }
    return HitTarget::None;
}

static bool IsSignatureArea(const ImVec2& point) {
    return PointInRect(point, 50, 150, 550, 270);
}

static std::string NormalizeRFIDUID(const std::string& uid) {
    std::string normalized = APIClient::CleanRFID(uid);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    const char* override_value = std::getenv("BITS_BYTES_RFID_UID_OVERRIDE");
    if (override_value != NULL && override_value[0] != '\0') {
        normalized = APIClient::CleanRFID(override_value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
    }
    return normalized;
}

static void QueueIdleLights(AppContext& context) {
    context.rfid_reader.CancelPendingFeedback();
    context.rfid_reader.QueueCommand("green_on");
    context.rfid_reader.QueueCommand("red_off");
}

static void QueueClockInLights(AppContext& context) {
    context.rfid_reader.QueueCommand("buzz");
    context.rfid_reader.QueueCommand("red_on");
    context.rfid_reader.QueueCommand("green_off");
}

static void RequireCardRemoval(AppContext& context, const std::string& uid) {
    context.require_card_removal = true;
    context.removal_uid = uid;
}

static void ReturnToWaiting(AppContext& context, double now_seconds) {
    const bool keep_removal = context.card_presence.IsPresent();
    const std::string present_uid = context.card_presence.UID();
    context.ResetWorkflow();
    if (keep_removal) {
        RequireCardRemoval(context, present_uid);
    } else {
        context.require_card_removal = false;
        context.removal_uid.clear();
    }
    QueueIdleLights(context);
    context.ChangeState(STATE_WAITING_CARD, now_seconds);
}

static void ShowError(AppContext& context,
                      const std::string& message,
                      bool uncertain,
                      double now_seconds) {
    context.message = message.empty() ? "Er is iets misgegaan" : message;
    context.error_detail = uncertain
        ? (context.card_presence.IsPresent()
            ? "Verwijder de kaart en scan opnieuw"
            : "Scan de kaart opnieuw")
        : std::string();
    if (uncertain && context.card_presence.IsPresent()) {
        RequireCardRemoval(context, context.pending_rfid_uid);
    }
    printf("- %s%s\n",
           context.message.c_str(),
           uncertain ? "; automatic retry disabled" : "");
    context.ChangeState(STATE_ERROR, now_seconds);
}

static bool SubmitJob(AppContext& context,
                      WorkflowRequest kind,
                      uint64_t id,
                      const std::string& processing_message,
                      double now_seconds) {
    if (id == 0) {
        ShowError(context, "Verzoek kon niet worden gestart", false, now_seconds);
        return false;
    }
    context.request_kind = kind;
    context.request_id = id;
    context.processing_message = processing_message;
    context.ChangeState(STATE_PROCESSING, now_seconds);
    return true;
}

static void HandleApiResult(AppContext& context, const ApiResult& result, double now_seconds) {
    if (result.type == ApiJobType::Health) {
        context.health_known = true;
        context.health_online = result.IsOk();
        context.next_health_check = now_seconds + (result.IsOk() ? 60.0 : 15.0);
        if (context.api_worker.IsMockMode()) {
            printf("+ Mock API ready (no network requests)\n");
        } else {
            printf("%c API %s: %s%s\n",
                   result.IsOk() ? '+' : '-',
                   result.IsOk() ? "reachable" : "unavailable",
                   context.api_worker.GetBaseURL().c_str(),
                   context.api_worker.HasAPIKey() ? " (Authorization configured)" : " (no API key configured)");
        }
        return;
    }
    if (!IsCurrentApiResult(context.request_id, result)) return;

    context.health_known = true;
    context.health_online = result.http_code > 0;
    context.next_health_check = now_seconds + (context.health_online ? 60.0 : 15.0);

    const WorkflowRequest completed_kind = context.request_kind;
    context.request_id = 0;
    context.request_kind = WorkflowRequest::None;
    context.processing_message.clear();

    if (!result.IsOk()) {
        if (completed_kind == WorkflowRequest::Attendance && result.outcome != ApiOutcome::Cancelled) {
            context.attendance_dates.clear();
            context.attendance_warning = "Aanwezigheid niet beschikbaar";
            context.ChangeState(STATE_ATTENDANCE, now_seconds);
            return;
        }
        const bool uncertain = result.outcome == ApiOutcome::Uncertain;
        ShowError(context, result.message, uncertain, now_seconds);
        return;
    }

    if (completed_kind == WorkflowRequest::Scan) {
        context.user_name = result.scan.user_name;
        context.user_department = result.scan.user_department;
        context.action = result.scan.action;
        context.message = result.scan.message;
        printf("+ %s - %s\n", context.action.c_str(), context.user_name.c_str());

        if (context.action == "clock_out") {
            context.rfid_reader.QueueCommand("beep");
            RequireCardRemoval(context, context.pending_rfid_uid);
            context.ChangeState(STATE_SUCCESS, now_seconds);
        } else {
            QueueClockInLights(context);
            SubmitJob(context,
                      WorkflowRequest::Attendance,
                      context.api_worker.SubmitAttendance(context.pending_rfid_uid),
                      "Aanwezigheid laden...",
                      now_seconds);
        }
        return;
    }

    if (completed_kind == WorkflowRequest::Attendance) {
        context.attendance_dates = result.attendance_dates;
        context.attendance_warning.clear();
        context.ChangeState(STATE_ATTENDANCE, now_seconds);
        return;
    }

    if (completed_kind == WorkflowRequest::Signature) {
        context.rfid_reader.QueueCommand("buzz");
        RequireCardRemoval(context, context.pending_rfid_uid);
        context.ChangeState(STATE_SUCCESS, now_seconds);
    }
}

static void DrainApiResults(AppContext& context, double now_seconds) {
    ApiResult result;
    while (context.api_worker.TryPop(result)) HandleApiResult(context, result, now_seconds);
}

static void DrainRFID(AppContext& context, double now_seconds) {
    RFIDData card;
    while (context.rfid_reader.PopCard(card)) {
        if (!card.valid) continue;
        const std::string uid = NormalizeRFIDUID(card.uid);
        if (uid.empty() || uid.size() > 64) {
            if (uid.size() > 64) fprintf(stderr, "- Ignoring oversized RFID UID\n");
            continue;
        }
        const bool new_presentation = context.card_presence.Observe(uid, now_seconds);
        context.activity_detected = true;

        if (context.current_state == STATE_ADMIN) {
            context.admin_displayed_rfid = uid;
            RequireCardRemoval(context, uid);
            continue;
        }
        if (context.current_state != STATE_WAITING_CARD || context.ApiBusy()) continue;
        if (!new_presentation) continue;
        if (context.require_card_removal) {
            if (uid == context.removal_uid) continue;
            context.require_card_removal = false;
            context.removal_uid.clear();
        }

        context.pending_rfid_uid = uid;
        RequireCardRemoval(context, uid);
        printf("RFID Card: %s\n", uid.c_str());
        SubmitJob(context,
                  WorkflowRequest::Scan,
                  context.api_worker.SubmitScan(uid),
                  "Kaart verwerken...",
                  now_seconds);
    }

    if (context.card_presence.Update(now_seconds)) {
        if (context.require_card_removal) {
            context.require_card_removal = false;
            context.removal_uid.clear();
        }
    }
}

static void ActivateTarget(AppContext& context, HitTarget target, double now_seconds) {
    switch (target) {
        case HitTarget::Admin:
            context.admin_password_buffer.clear();
            context.ChangeState(STATE_ADMIN_PASSWORD, now_seconds);
            break;
        case HitTarget::AttendanceBack:
            ReturnToWaiting(context, now_seconds);
            break;
        case HitTarget::AttendanceConfirm:
            context.signature.Clear();
            context.signature_warning.clear();
            context.ChangeState(STATE_SIGNATURE, now_seconds);
            break;
        case HitTarget::SignatureClear:
            context.signature.Clear();
            context.signature_warning.clear();
            break;
        case HitTarget::SignatureCancel:
            ReturnToWaiting(context, now_seconds);
            break;
        case HitTarget::SignatureSubmit: {
            context.signature_warning.clear();
            if (context.signature.TooComplex()) {
                context.signature_warning = "Handtekening is te complex; wis en probeer opnieuw";
                break;
            }
            std::string svg;
            if (!context.signature.SerializeSVG(svg)) {
                context.signature_warning = context.signature.IsValid()
                    ? "Handtekening is te groot; wis en probeer opnieuw"
                    : "Teken eerst uw handtekening";
                break;
            }
            SubmitJob(context,
                      WorkflowRequest::Signature,
                      context.api_worker.SubmitSignature(context.pending_rfid_uid, svg),
                      "Handtekening versturen...",
                      now_seconds);
            break;
        }
        case HitTarget::AdminBack:
            context.admin_password_buffer.clear();
            ReturnToWaiting(context, now_seconds);
            break;
        default:
            if (target >= HitTarget::Key1 && target <= HitTarget::Key9) {
                const int digit = static_cast<int>(target) - static_cast<int>(HitTarget::Key1) + 1;
                if (context.admin_password_buffer.size() < ADMIN_PASSWORD.size()) {
                    context.admin_password_buffer += static_cast<char>('0' + digit);
                    context.admin_last_digit = digit;
                    context.admin_last_digit_time = ImGui::GetTime();
                }
                if (context.admin_password_buffer.size() == ADMIN_PASSWORD.size()) {
                    if (context.admin_password_buffer == ADMIN_PASSWORD) {
                        context.admin_displayed_rfid.clear();
                        context.ChangeState(STATE_ADMIN, now_seconds);
                    } else {
                        context.admin_password_buffer.clear();
                        ShowError(context, "Pincode onjuist", false, now_seconds);
                    }
                }
            }
            break;
    }
}

static void HandleTouch(AppContext& context, double now_seconds) {
    const std::vector<TouchEvent>& events = context.touch_handler.Events();
    for (size_t i = 0; i < events.size(); ++i) {
        const TouchEvent event = events[i];
        context.activity_detected = true;

        if (context.current_state == STATE_PROCESSING ||
            context.current_state == STATE_SUCCESS ||
            context.current_state == STATE_ERROR) {
            continue;
        }

        const bool signature_stroke_event = context.current_state == STATE_SIGNATURE &&
            (context.signature.IsDrawing() ||
             (event.type == TouchEventType::Down && IsSignatureArea(event.position)));
        if (signature_stroke_event) {
            if (event.type == TouchEventType::Down) {
                context.gesture.Clear();
                context.button_feedback.CancelPress();
                context.signature.Begin(event.position);
            } else if (event.type == TouchEventType::Move && context.signature.IsDrawing()) {
                context.signature.Add(event.position);
            } else if (event.type == TouchEventType::Up && context.signature.IsDrawing()) {
                context.signature.End();
                if (context.signature.TooComplex()) {
                    context.signature_warning = "Handtekening is te complex; wis en probeer opnieuw";
                }
            } else if (event.type == TouchEventType::Cancel) {
                context.signature.CancelStroke();
            }
            continue;
        }

        if (event.type == TouchEventType::Down) {
            context.gesture.active = true;
            context.gesture.moved = false;
            context.gesture.start = event.position;
            context.gesture.last = event.position;
            context.gesture.target = TargetAt(context.current_state, event.position);
            context.button_feedback.Press(context.gesture.target);
            if (context.current_state == STATE_ATTENDANCE &&
                PointInRect(event.position, 40, 90, 560, 330)) {
                context.attendance_dragging = true;
            }
        } else if (event.type == TouchEventType::Move && context.gesture.active) {
            if (Distance(event.position, context.gesture.start) > CLICK_MOVEMENT_LIMIT) {
                context.gesture.moved = true;
                context.button_feedback.CancelPress();
            }
            if (context.current_state == STATE_ATTENDANCE && context.attendance_dragging) {
                const float total_height = 24.0f * context.attendance_dates.size();
                const float max_scroll = std::max(0.0f, total_height - 280.0f);
                context.attendance_scroll_offset += event.position.y - context.gesture.last.y;
                context.attendance_scroll_offset = std::max(-max_scroll, std::min(0.0f, context.attendance_scroll_offset));
            }
            context.gesture.last = event.position;
        } else if (event.type == TouchEventType::Up && context.gesture.active) {
            const HitTarget release_target = TargetAt(context.current_state, event.position);
            const bool valid_click = !context.gesture.moved &&
                                     context.gesture.target != HitTarget::None &&
                                     release_target == context.gesture.target;
            const HitTarget activated = context.gesture.target;
            if (valid_click) {
                context.button_feedback.Release(activated, now_seconds);
            } else {
                context.button_feedback.CancelPress();
            }
            context.gesture.Clear();
            context.attendance_dragging = false;
            if (valid_click) ActivateTarget(context, activated, now_seconds);
        } else if (event.type == TouchEventType::Cancel) {
            context.gesture.Clear();
            context.button_feedback.CancelPress();
            context.attendance_dragging = false;
            context.signature.CancelStroke();
        }
    }
}

static int ReadBacklightMax() {
#ifdef DESKTOP_SIM
    return BACKLIGHT_FULL_DEFAULT;
#else
    const int descriptor = open(BACKLIGHT_MAX_PATH, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return BACKLIGHT_FULL_DEFAULT;
    char value[16] = {0};
    const ssize_t count = read(descriptor, value, sizeof(value) - 1);
    close(descriptor);
    const int parsed = count > 0 ? atoi(value) : 0;
    return parsed > 0 ? parsed : BACKLIGHT_FULL_DEFAULT;
#endif
}

static bool WriteBacklight(int value) {
#ifdef DESKTOP_SIM
    (void)value;
    return true;
#else
    const int descriptor = open(BACKLIGHT_BRIGHTNESS_PATH, O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) return false;
    char buffer[16];
    const int length = snprintf(buffer, sizeof(buffer), "%d", value);
    ssize_t offset = 0;
    while (offset < length) {
        const ssize_t written = write(descriptor, buffer + offset, static_cast<size_t>(length - offset));
        if (written > 0) {
            offset += written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    close(descriptor);
    return offset == length;
#endif
}

static void SetBacklightTarget(AppContext& context, int target) {
    target = std::max(0, std::min(context.backlight_max, target));
    if (target == context.backlight_target) return;
    context.backlight_transition_start = context.backlight_current_f < 0.0
        ? target
        : context.backlight_current_f;
    context.backlight_target = target;
    context.backlight_transition_progress = 0.0;
}

static void UpdateBacklight(AppContext& context, double delta, double now_seconds) {
    if (context.activity_detected) {
        context.inactivity_seconds = 0.0;
        SetBacklightTarget(context, std::min(BACKLIGHT_FULL_DEFAULT, context.backlight_max));
    } else {
        context.inactivity_seconds += delta;
        if (context.inactivity_seconds >= BACKLIGHT_OFF_SECONDS) {
            SetBacklightTarget(context, 0);
        } else if (context.inactivity_seconds >= BACKLIGHT_DIM_SECONDS) {
            SetBacklightTarget(context, std::min(BACKLIGHT_DIM_VALUE, context.backlight_max));
        }
    }

    if (context.backlight_target < 0) return;
    context.backlight_transition_progress = std::min(
        1.0, context.backlight_transition_progress + delta / BACKLIGHT_TRANSITION_SECONDS);
    context.backlight_current_f = context.backlight_transition_start +
        (context.backlight_target - context.backlight_transition_start) * context.backlight_transition_progress;
    const int value = static_cast<int>(context.backlight_current_f + 0.5);
    if (value != context.backlight_current && now_seconds - context.backlight_last_write >= 0.05) {
        if (WriteBacklight(value)) context.backlight_current = value;
        context.backlight_last_write = now_seconds;
    }
}

static void UpdateApp(AppContext& context, double delta, double now_seconds) {
    context.activity_detected = false;
    context.touch_handler.Update(now_seconds);
    context.rfid_reader.Update(now_seconds);
    DrainApiResults(context, now_seconds);
    DrainRFID(context, now_seconds);
    HandleTouch(context, now_seconds);

    if (now_seconds >= context.next_health_check &&
        !context.ApiBusy() && context.current_state == STATE_WAITING_CARD) {
        context.api_worker.SubmitHealth();
        context.next_health_check = now_seconds + 15.0;
    }

    if ((context.current_state == STATE_SUCCESS || context.current_state == STATE_ERROR) &&
        now_seconds - context.state_started >= MESSAGE_DURATION_SECONDS) {
        ReturnToWaiting(context, now_seconds);
    }
    UpdateBacklight(context, delta, now_seconds);
}

static void RenderPerformanceOverlay(const AppContext& context) {
    if (!context.performance.Enabled()) return;
    const PerformanceSnapshot& metrics = context.performance.Snapshot();
    char touch_latency[48];
    if (metrics.touch_latency_ms < 0.0) {
        snprintf(touch_latency, sizeof(touch_latency), "n/a");
    } else {
        snprintf(touch_latency, sizeof(touch_latency), "%.2f ms", metrics.touch_latency_ms);
    }
    char lines[768];
    snprintf(lines, sizeof(lines),
             "Display: %d Hz (VSync)   FPS: %.1f\n"
             "Frame ms p50/p95/p99: %.2f / %.2f / %.2f\n"
             "Update/render/swap p95: %.2f / %.2f / %.2f ms\n"
             "Touch latency: %s   Signature raw/kept: %zu / %zu\n"
             "Draw: %d calls, %d vertices, %d indices\n"
             "Touch/RFID reconnects: %u / %u   API: %s",
             context.display_refresh_hz,
             metrics.fps,
             metrics.frame_p50_ms,
             metrics.frame_p95_ms,
             metrics.frame_p99_ms,
             metrics.update_p95_ms,
             metrics.render_p95_ms,
             metrics.swap_p95_ms,
             touch_latency,
             context.signature.RawSampleCount(),
             context.signature.TotalPointCount(),
             metrics.draw_calls,
             metrics.vertices,
             metrics.indices,
             context.touch_handler.ReconnectCount(),
             context.rfid_reader.ReconnectCount(),
             context.ApiBusy() ? "busy" : (context.health_online ? "online" : "offline"));
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(8, 8), ImVec2(460, 155), IM_COL32(15, 15, 15, 225), 5.0f);
    draw->AddText(ImVec2(16, 15), IM_COL32(255, 255, 255, 255), lines);
}

static void RenderApp(AppContext& context, double now_seconds) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    context.ui_renderer.BeginFrame(&context.button_feedback, now_seconds);
    switch (context.current_state) {
        case STATE_WAITING_CARD:
            context.ui_renderer.RenderWaitingScreen(draw);
            if (context.health_known && !context.health_online) {
                draw->AddText(ImVec2(18, 448), BitsBytesTheme::RedText,
                              "API offline - nieuwe scans kunnen mislukken");
            }
            break;
        case STATE_PROCESSING:
            context.ui_renderer.RenderProcessingScreen(draw, context.processing_message);
            break;
        case STATE_ATTENDANCE:
            context.ui_renderer.RenderAttendanceScreen(draw,
                                                       context.user_name,
                                                       context.attendance_dates,
                                                       context.attendance_warning,
                                                       context.attendance_scroll_offset);
            break;
        case STATE_SIGNATURE:
            context.ui_renderer.RenderSignatureScreen(draw,
                                                      context.user_name,
                                                      context.signature.Strokes(),
                                                      context.signature.CurrentStroke(),
                                                      context.signature_warning);
            break;
        case STATE_SUCCESS:
            context.ui_renderer.RenderSuccessScreen(draw, context.user_name, context.action);
            break;
        case STATE_ERROR:
            context.ui_renderer.RenderErrorScreen(draw, context.message, context.error_detail);
            break;
        case STATE_ADMIN_PASSWORD:
            context.ui_renderer.RenderAdminPasswordScreen(draw,
                                                          context.admin_password_buffer,
                                                          context.admin_last_digit,
                                                          static_cast<float>(context.admin_last_digit_time));
            break;
        case STATE_ADMIN:
            context.ui_renderer.RenderAdminScreen(draw, context.admin_displayed_rfid);
            break;
    }
    RenderPerformanceOverlay(context);
}

#ifdef DESKTOP_SIM
static const char* DEFAULT_SIM_RFID_UID = "11F3EF12";
static bool g_show_simulator_help = true;
static std::string g_last_simulated_card = "none";

static bool CaptureSimulatorFrame(const std::string& path, int width, int height) {
    if (path.empty() || width <= 0 || height <= 0) return false;
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
    std::vector<unsigned char> rgb(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    for (int output_y = 0; output_y < height; ++output_y) {
        const int source_y = height - output_y - 1;
        for (int x = 0; x < width; ++x) {
            const size_t source = (static_cast<size_t>(source_y) * width + x) * 4;
            const size_t destination = (static_cast<size_t>(output_y) * width + x) * 3;
            rgb[destination] = rgba[source];
            rgb[destination + 1] = rgba[source + 1];
            rgb[destination + 2] = rgba[source + 2];
        }
    }
    FILE* output = fopen(path.c_str(), "wb");
    if (output == NULL) return false;
    fprintf(output, "P6\n%d %d\n255\n", width, height);
    const size_t written = fwrite(rgb.data(), 1, rgb.size(), output);
    const bool closed = fclose(output) == 0;
    return written == rgb.size() && closed;
}

static const char* StateName(AppState state) {
    switch (state) {
        case STATE_WAITING_CARD: return "waiting";
        case STATE_PROCESSING: return "processing";
        case STATE_ATTENDANCE: return "attendance";
        case STATE_SIGNATURE: return "signature";
        case STATE_SUCCESS: return "success";
        case STATE_ERROR: return "error";
        case STATE_ADMIN_PASSWORD: return "admin PIN";
        case STATE_ADMIN: return "admin";
    }
    return "unknown";
}

static void ResetSimulation(AppContext& context, double now_seconds) {
    context.card_presence.ForceAbsent();
    context.require_card_removal = false;
    context.removal_uid.clear();
    context.ResetWorkflow();
    QueueIdleLights(context);
    context.ChangeState(STATE_WAITING_CARD, now_seconds);
}

static void InjectSimulationCard(AppContext& context,
                                 const std::string& uid,
                                 bool reset,
                                 double now_seconds) {
    if (uid.empty()) return;
    if (context.ApiBusy() || (reset && context.current_state != STATE_WAITING_CARD)) {
        printf("- [simulator] card ignored while a workflow is active\n");
        return;
    }
    if (reset) ResetSimulation(context, now_seconds);
    context.rfid_reader.InjectCard(uid);
    g_last_simulated_card = uid;
    printf("+ [simulator] card injected: %s\n", uid.c_str());
}

static std::string ConfiguredSimulationUID(const AppContext& context, const char* mock_uid) {
    const char* configured = std::getenv("STM32_SIM_RFID_UID");
    if (configured != NULL && configured[0] != '\0') return configured;
    return context.api_worker.IsMockMode() ? mock_uid : DEFAULT_SIM_RFID_UID;
}

static void HandleSimulatorHotkeys(AppContext& context, double now_seconds) {
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        InjectSimulationCard(context, ConfiguredSimulationUID(context, "SIM_CLOCK_IN"), true, now_seconds);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        InjectSimulationCard(context, ConfiguredSimulationUID(context, "SIM_CLOCK_OUT"), true, now_seconds);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
        InjectSimulationCard(context, "SIM_UNKNOWN", true, now_seconds);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F4, false)) {
        InjectSimulationCard(context, ConfiguredSimulationUID(context, "SIM_ADMIN_CARD"), false, now_seconds);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (context.ApiBusy()) {
            printf("- [simulator] reset ignored while an API write may be in flight\n");
        } else {
            ResetSimulation(context, now_seconds);
            g_last_simulated_card = "none";
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) g_show_simulator_help = !g_show_simulator_help;
}

static void RenderSimulatorHelp(const AppContext& context) {
    if (!g_show_simulator_help || context.performance.Enabled()) return;
    char first[192];
    char second[192];
    snprintf(first, sizeof(first), "WSL simulator | API: %s | state: %s",
             context.api_worker.IsMockMode() ? "MOCK" : "LIVE", StateName(context.current_state));
    snprintf(second, sizeof(second), "Mouse = touch | last card: %s", g_last_simulated_card.c_str());
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(8, 8), ImVec2(430, 112), IM_COL32(20, 20, 20, 225), 5.0f);
    draw->AddText(ImVec2(16, 14), IM_COL32_WHITE, first);
    draw->AddText(ImVec2(16, 38), IM_COL32_WHITE,
                  context.api_worker.IsMockMode() ? "F1 clock-in  F2 clock-out  F3 unknown" : "F1/F2 scan 11F3EF12  F3 unknown");
    draw->AddText(ImVec2(16, 62), IM_COL32_WHITE, "F4 held-card injection  F5 reset  F12 help");
    draw->AddText(ImVec2(16, 86), IM_COL32_WHITE, second);
}
#endif

static std::string FramebufferPreference() {
    const char* configured = std::getenv("BITS_BYTES_FRAMEBUFFER_FORMAT");
#ifdef DESKTOP_SIM
    const std::string fallback = "rgba8888";
#else
    const std::string fallback = "auto";
#endif
    if (configured == NULL || configured[0] == '\0') return fallback;
    const std::string value = configured;
    if (value == "auto" || value == "rgb565" || value == "rgba8888") return value;
    fprintf(stderr, "- Unknown BITS_BYTES_FRAMEBUFFER_FORMAT '%s'; using %s\n",
            configured, fallback.c_str());
    return fallback;
}

static void SetCommonWindowHints() {
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_SAMPLES, 0);
}

static GLFWwindow* CreateApplicationWindow(GLFWmonitor* monitor,
                                           const char* title,
                                           std::string& actual_format) {
    const std::string preference = FramebufferPreference();
    const bool try_rgb565 = preference == "auto" || preference == "rgb565";
    if (try_rgb565) {
        SetCommonWindowHints();
        glfwWindowHint(GLFW_RED_BITS, 5);
        glfwWindowHint(GLFW_GREEN_BITS, 6);
        glfwWindowHint(GLFW_BLUE_BITS, 5);
        glfwWindowHint(GLFW_ALPHA_BITS, 0);
        GLFWwindow* window = glfwCreateWindow(800, 480, title, monitor, NULL);
        if (window != NULL) {
            actual_format = "rgb565";
            return window;
        }
        fprintf(stderr, "- RGB565 EGL configuration unavailable; falling back to RGBA8888\n");
    }

    SetCommonWindowHints();
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    GLFWwindow* window = glfwCreateWindow(800, 480, title, monitor, NULL);
    if (window != NULL) actual_format = "rgba8888";
    return window;
}

int main(int argc, char** argv) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "- Failed to initialize libcurl\n");
        return 1;
    }

    printf("================================================\n");
    printf("RFID Attendance System\n");
#ifdef DESKTOP_SIM
    printf("WSL desktop hardware simulation\n");
#endif
    printf("================================================\n");

    AppContext context;
    context.backlight_max = ReadBacklightMax();
    context.backlight_current_f = std::min(BACKLIGHT_FULL_DEFAULT, context.backlight_max);
    context.backlight_target = static_cast<int>(context.backlight_current_f);
    WriteBacklight(context.backlight_target);
    context.backlight_current = context.backlight_target;
    if (context.api_worker.Start()) {
        context.api_worker.SubmitHealth();
    } else {
        fprintf(stderr, "- API worker could not start; UI will remain available offline\n");
        context.health_known = true;
        context.health_online = false;
    }
    context.next_health_check = SteadySeconds() + 15.0;

    if (!glfwInit()) {
        fprintf(stderr, "- Failed to initialize GLFW\n");
        context.api_worker.Stop();
        curl_global_cleanup();
        return 1;
    }

#ifdef DESKTOP_SIM
    GLFWmonitor* monitor = NULL;
    const char* title = "RFID Attendance - WSL Simulator";
#else
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const char* title = "RFID Attendance";
#endif

    std::string framebuffer_format;
    GLFWwindow* window = CreateApplicationWindow(monitor, title, framebuffer_format);
    if (window == NULL) {
        fprintf(stderr, "- Failed to create EGL window (requested %s)\n", FramebufferPreference().c_str());
        glfwTerminate();
        context.api_worker.Stop();
        curl_global_cleanup();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    GLint actual_red_bits = 0;
    GLint actual_green_bits = 0;
    GLint actual_blue_bits = 0;
    GLint actual_alpha_bits = 0;
    glGetIntegerv(GL_RED_BITS, &actual_red_bits);
    glGetIntegerv(GL_GREEN_BITS, &actual_green_bits);
    glGetIntegerv(GL_BLUE_BITS, &actual_blue_bits);
    glGetIntegerv(GL_ALPHA_BITS, &actual_alpha_bits);
    GLFWmonitor* refresh_monitor = monitor != NULL ? monitor : glfwGetPrimaryMonitor();
    const GLFWvidmode* video_mode = refresh_monitor != NULL ? glfwGetVideoMode(refresh_monitor) : NULL;
    context.display_refresh_hz = video_mode != NULL ? video_mode->refreshRate : 0;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // The kiosk has no persistent ImGui window layout. Avoid periodic writes
    // to the board filesystem (which may also be mounted read-only).
    io.IniFilename = NULL;
    io.LogFilename = NULL;
#ifdef DESKTOP_SIM
    io.MouseDrawCursor = true;
#else
    io.MouseDrawCursor = false;
#endif
    BrandedFontBundle fonts = AddBrandedFonts(io, 20.0f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 100");

    // ImGui 1.92 selects the dynamic atlas path on its first frame after the
    // renderer backend advertises texture updates. Synchronize that state
    // before packing the generated icon masks into the same texture.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::EndFrame();
    InstallBrandedIcons(io, fonts, 20.0f);
    context.ui_renderer.SetFonts(fonts.regular, fonts.semibold, fonts.icons_installed);

    printf("%c Loaded embedded fonts: Outfit Regular + SemiBold\n",
           fonts.regular != NULL && fonts.semibold != NULL ? '+' : '-');
    printf("%c Branded icon atlas: %s (%dx%d RGBA)\n",
           fonts.icons_installed ? '+' : '-',
           fonts.icons_installed ? "ready" : "fallback labels",
           fonts.atlas_width,
           fonts.atlas_height);
    printf("+ Framebuffer request: %s\n", framebuffer_format.c_str());
    printf("+ Framebuffer actual: R%d G%d B%d A%d\n",
           actual_red_bits, actual_green_bits, actual_blue_bits, actual_alpha_bits);
    printf("+ Active display refresh: %d Hz (VSync enabled)\n", context.display_refresh_hz);
    printf("+ API mode: %s, URL: %s\n",
           context.api_worker.IsMockMode() ? "MOCK" : "LIVE",
           context.api_worker.GetBaseURL().c_str());
    printf("+ Performance overlay: %s\n", context.performance.Enabled() ? "enabled" : "disabled");
    printf("+ System ready\n");

#ifdef DESKTOP_SIM
    double simulator_exit_after = 0.0;
    int simulator_signature_points = 0;
    std::string simulator_screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const std::string rfid_prefix = "--sim-rfid=";
        const std::string exit_prefix = "--sim-exit-after=";
        const std::string signature_prefix = "--sim-signature-points=";
        const std::string screenshot_prefix = "--sim-screenshot=";
        if (argument.compare(0, rfid_prefix.size(), rfid_prefix) == 0) {
            InjectSimulationCard(context, argument.substr(rfid_prefix.size()), true, SteadySeconds());
        } else if (argument.compare(0, exit_prefix.size(), exit_prefix) == 0) {
            simulator_exit_after = std::max(0.0, atof(argument.substr(exit_prefix.size()).c_str()));
        } else if (argument.compare(0, signature_prefix.size(), signature_prefix) == 0) {
            simulator_signature_points = std::max(0, atoi(argument.substr(signature_prefix.size()).c_str()));
        } else if (argument.compare(0, screenshot_prefix.size(), screenshot_prefix) == 0) {
            simulator_screenshot = argument.substr(screenshot_prefix.size());
            g_show_simulator_help = false;
        }
    }
    if (simulator_signature_points > 0) {
        context.ResetWorkflow();
        context.user_name = "Signature benchmark";
        context.pending_rfid_uid = "SIM_BENCHMARK";
        context.action = "clock_in";
        context.ChangeState(STATE_SIGNATURE, SteadySeconds());
        context.signature.Begin(ImVec2(60.0f, 285.0f));
        const int point_limit = std::min(simulator_signature_points,
                                         static_cast<int>(SignaturePad::MAX_POINTS - 1));
        for (int point = 1; point <= point_limit; ++point) {
            const float x = 60.0f + static_cast<float>(point % 530);
            const float y = 285.0f + std::sin(point * 0.17f) * 115.0f;
            context.signature.Add(ImVec2(x, y));
        }
        printf("+ Simulator signature benchmark: %zu retained points\n",
               context.signature.TotalPointCount());
    }
#else
    (void)argc;
    (void)argv;
#endif

    QueueIdleLights(context);
#ifdef DESKTOP_SIM
    const double application_started = SteadySeconds();
#endif
    double last_frame = SteadySeconds();

    while (!glfwWindowShouldClose(window)) {
        const double frame_start = SteadySeconds();
        const double frame_seconds = std::max(0.0, frame_start - last_frame);
        const double delta = std::min(0.05, frame_seconds);
        last_frame = frame_start;

        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

#ifdef DESKTOP_SIM
        HandleSimulatorHotkeys(context, frame_start);
#endif

        const double update_start = SteadySeconds();
        UpdateApp(context, delta, frame_start);
        const double render_start = SteadySeconds();
        RenderApp(context, frame_start);
#ifdef DESKTOP_SIM
        RenderSimulatorHelp(context);
#endif
        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(247.0f / 255.0f, 251.0f / 255.0f, 1.0f, 1.0f);
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#ifdef DESKTOP_SIM
        if (!simulator_screenshot.empty() && frame_start - application_started >= 0.5) {
            const bool captured = CaptureSimulatorFrame(simulator_screenshot, width, height);
            printf("%c Simulator screenshot: %s\n",
                   captured ? '+' : '-', simulator_screenshot.c_str());
            simulator_screenshot.clear();
        }
#endif
        const double swap_start = SteadySeconds();
        glfwSwapBuffers(window);
        const double frame_end = SteadySeconds();

        ImDrawData* draw_data = ImGui::GetDrawData();
        int draw_calls = 0;
        if (draw_data != NULL) {
            for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
                draw_calls += draw_data->CmdLists[list_index]->CmdBuffer.Size;
            }
        }
        context.performance.Add(frame_end,
                                frame_seconds,
                                (render_start - update_start) * 1000.0,
                                (swap_start - render_start) * 1000.0,
                                (frame_end - swap_start) * 1000.0,
                                context.touch_handler.LastSampleLatencyMs(frame_end),
                                draw_data != NULL ? draw_data->TotalVtxCount : 0,
                                draw_data != NULL ? draw_data->TotalIdxCount : 0,
                                draw_calls);
#ifdef DESKTOP_SIM
        if (simulator_exit_after > 0.0 && frame_end - application_started >= simulator_exit_after) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
#endif
    }

#ifdef DESKTOP_SIM
    if (context.performance.Enabled()) {
        const PerformanceSnapshot& final_metrics = context.performance.Snapshot();
        printf("+ Simulator metrics: FPS %.1f, frame p95 %.2f ms, p99 %.2f ms, update p95 %.2f ms\n",
               final_metrics.fps,
               final_metrics.frame_p95_ms,
               final_metrics.frame_p99_ms,
               final_metrics.update_p95_ms);
    }
#endif
    context.api_worker.Stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    context.rfid_reader.Close();
    curl_global_cleanup();
    return 0;
}
