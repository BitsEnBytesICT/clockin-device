// main.cpp
// RFID Attendance System with Signature for Clock-In
// Clean architecture with state management

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "rfid_reader.h"
#include "touch_handler.h"
#include <curl/curl.h>
#include "api_client.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/input.h>
#include "assets/fonts/plus_jakarta_sans_wght.h"

// Numeric keypad touch areas (password screen) - MUST BE BEFORE ui_renderer.h
#define KEYPAD_BUTTON_W 120
#define KEYPAD_BUTTON_H 80
#define KEYPAD_START_X 205  // Centered for tighter spacing
#define KEYPAD_START_Y 190
#define KEYPAD_SPACING_X 115  // Closer spacing
#define KEYPAD_SPACING_Y 90

// RFID removal detection: number of consecutive empty polls to consider the card removed
#define RFID_REMOVE_FRAMES 10

// Attendance screen button areas
#define ATTENDANCE_CONFIRM_X 650
#define ATTENDANCE_CONFIRM_Y 400
#define ATTENDANCE_CONFIRM_W 140
#define ATTENDANCE_CONFIRM_H 60

#define ATTENDANCE_BACK_X 650
#define ATTENDANCE_BACK_Y 10
#define ATTENDANCE_BACK_W 140
#define ATTENDANCE_BACK_H 60

// Backlight control (STM32MP157F-DK2)
#define BACKLIGHT_BRIGHTNESS_PATH "/sys/class/backlight/5a000000.dsi.0/brightness"
#define BACKLIGHT_MAX_PATH "/sys/class/backlight/5a000000.dsi.0/max_brightness"
#define BACKLIGHT_FULL_DEFAULT 240
#define BACKLIGHT_DIM_VALUE 120
#define BACKLIGHT_DIM_SECONDS 30.0f
#define BACKLIGHT_OFF_SECONDS 60.0f
#define BACKLIGHT_TRANSITION_SECONDS 0.5f

#include "ui_renderer.h"

// ====================================================
// APPLICATION STATE
// ====================================================

enum AppState {
    STATE_WAITING_CARD,      // Waiting for RFID card
    STATE_ATTENDANCE,        // Show last 30 days before signature
    STATE_SIGNATURE,         // Drawing signature (clock in only)
    STATE_SUCCESS,           // Show success message
    STATE_ERROR,             // Show error message
    STATE_ADMIN_PASSWORD,    // Prompt for admin password
    STATE_ADMIN              // Admin menu
};

struct AppContext {
    AppState current_state;
    AppState next_state;
    
    RFIDReader rfid_reader;
    TouchHandler touch_handler;
    APIClient api_client;
    UIRenderer ui_renderer;
    
    // User data
    std::string pending_rfid_uid;
    std::string user_name;
    std::string user_department;
    std::string action;  // "clock_in" or "clock_out"
    std::string message;

    // Attendance data
    std::vector<std::string> attendance_dates;
    std::string attendance_warning;
    bool attendance_fetch_failed;
    float attendance_scroll_offset;
    bool attendance_is_dragging;
    bool attendance_dragged;
    ImVec2 attendance_last_touch_pos;
    TouchState attendance_touch;
    
    // Signature data
    std::vector<std::vector<ImVec2>> signature_strokes;
    std::vector<ImVec2> current_stroke;
    bool is_drawing;
    
    // Timing
    float state_timer;
    float message_duration;
    // Backlight
    float inactivity_timer;
    bool activity_detected;
    int backlight_max;
    int backlight_current;
    float backlight_current_f;
    int backlight_target;
    float backlight_transition_t;
    float backlight_transition_start;
    // Admin
    std::string admin_password_buffer;  // For PIN entry (numeric)
    bool admin_submit_requested;
    char admin_input_buf[128];
    std::string admin_displayed_rfid;
    ImVec2 admin_touch_start_pos;  // Track start position for button detection
    bool admin_was_touching;       // Track if touch was active
    int admin_last_digit;          // Last keypad digit pressed (1-9), -1 for none
    float admin_last_digit_time;   // Time of last digit press
    // Attendance screen touch state
    ImVec2 attendance_touch_start_pos;
    bool attendance_was_touching;
    // RFID debouncing - only poll on waiting screen
    std::string last_processed_rfid_uid;  // Track the card UID that was just processed to prevent duplicates while held
    bool rfid_card_present;               // True while the same card remains in the field
    int rfid_no_data_frames;              // Consecutive empty polls used to detect card removal
    
    AppContext() : 
        current_state(STATE_WAITING_CARD),
        next_state(STATE_WAITING_CARD),
        is_drawing(false),
        state_timer(0.0f),
        message_duration(3.0f),
        inactivity_timer(0.0f),
        activity_detected(false),
        backlight_max(BACKLIGHT_FULL_DEFAULT),
        backlight_current(-1),
        backlight_current_f(-1.0f),
        backlight_target(-1),
        backlight_transition_t(1.0f),
        backlight_transition_start(0.0f),
        admin_submit_requested(false),
        admin_was_touching(false),
        rfid_card_present(false),
        rfid_no_data_frames(0),
        admin_last_digit(-1),
        admin_last_digit_time(0.0f),
        attendance_was_touching(false),
        attendance_fetch_failed(false),
        attendance_scroll_offset(0.0f),
        attendance_is_dragging(false),
        attendance_dragged(false),
        attendance_last_touch_pos(0.0f, 0.0f) {
            admin_input_buf[0] = '\0';
        }
    
    void ChangeState(AppState new_state) {
        next_state = new_state;
        state_timer = 0.0f;
        
        // Clear RFID debounce when entering WAITING state (fresh start for new card reads)
        if (new_state == STATE_WAITING_CARD) {
            // Drain any buffered RFID data so we don't process stale reads
            rfid_reader.Flush();
            // Start removal detection fresh; keep last_processed_rfid_uid until card is removed
            rfid_no_data_frames = 0;
        }
        if (new_state == STATE_ADMIN_PASSWORD) {
            admin_last_digit = -1;
            admin_last_digit_time = 0.0f;
        }
    }
    
    void ClearSignature() {
        signature_strokes.clear();
        current_stroke.clear();
        is_drawing = false;
    }
    
    void Reset() {
        user_name.clear();
        user_department.clear();
        action.clear();
        message.clear();
        attendance_dates.clear();
        attendance_warning.clear();
        attendance_fetch_failed = false;
        attendance_scroll_offset = 0.0f;
        attendance_is_dragging = false;
        attendance_dragged = false;
        for (int i = 0; i < 10; ++i) {
            attendance_touch.slots[i].active = false;
            attendance_touch.slots[i].x = 0;
            attendance_touch.slots[i].y = 0;
        }
        attendance_touch.current_slot = 0;
        ClearSignature();
        admin_password_buffer.clear();
        admin_submit_requested = false;
        admin_displayed_rfid.clear();
        admin_input_buf[0] = '\0';
        admin_last_digit = -1;
        admin_last_digit_time = 0.0f;
        attendance_was_touching = false;
        // NOTE: DO NOT clear last_processed_rfid_uid here!
        // It is cleared only after the card is removed
    }
};

// Hardcoded admin PIN (numeric)
static const std::string ADMIN_PASSWORD = "1111";

// Helper to get keypad button area for digit (1-9)
bool IsInKeypadButton(float x, float y, int digit) {
    if (digit < 1 || digit > 9) return false;
    int row = (digit - 1) / 3;
    int col = (digit - 1) % 3;
    float btn_x = KEYPAD_START_X + col * KEYPAD_SPACING_X;
    float btn_y = KEYPAD_START_Y + row * KEYPAD_SPACING_Y;
    return (x >= btn_x && x <= btn_x + KEYPAD_BUTTON_W && 
            y >= btn_y && y <= btn_y + KEYPAD_BUTTON_H);
}

// Back button area on password screen
bool IsInPasswordBackButton(float x, float y) {
    return (x >= 650 && x <= 790 && y >= 10 && y <= 70);
}

// Back button area on admin screen (same location)
bool IsInAdminBackButton(float x, float y) {
    return (x >= 650 && x <= 790 && y >= 10 && y <= 70);
}

static ImFont* LoadJakartaSans(ImGuiIO& io, float size_pixels) {
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;

    ImFont* font = io.Fonts->AddFontFromMemoryTTF(
        _home_derk_imgui_stm32_project_assets_fonts_PlusJakartaSans_wght__ttf,
        _home_derk_imgui_stm32_project_assets_fonts_PlusJakartaSans_wght__ttf_len,
        size_pixels,
        &font_cfg
    );

    if (font != nullptr) {
        printf("+ Loaded embedded font: Plus Jakarta Sans\n");
        return font;
    }

    printf("- Embedded Jakarta Sans failed to load. Using default font.\n");
    return nullptr;
}

std::string NormalizeRFIDUID(const std::string& raw_uid) {
    std::string cleaned = APIClient::clean_rfid_uid(raw_uid);
    std::string upper;
    upper.reserve(cleaned.size());
    for (char c : cleaned) {
        upper.push_back((char)std::toupper((unsigned char)c));
    }

    // Test overrides must be explicit at runtime.  Never silently map a real
    // card to another participant when connected to the production backend.
    const char* configured_override = std::getenv("BITS_BYTES_RFID_UID_OVERRIDE");
    if (configured_override != nullptr && configured_override[0] != '\0') {
        std::string override_uid = APIClient::clean_rfid_uid(configured_override);
        std::transform(override_uid.begin(), override_uid.end(), override_uid.begin(), [](unsigned char c) {
            return (char)std::toupper(c);
        });
        return override_uid;
    }

    return upper;
}

// Confirm button area on attendance screen (bottom-right)
bool IsInAttendanceConfirmButton(float x, float y) {
    return (x >= ATTENDANCE_CONFIRM_X && x <= ATTENDANCE_CONFIRM_X + ATTENDANCE_CONFIRM_W &&
            y >= ATTENDANCE_CONFIRM_Y && y <= ATTENDANCE_CONFIRM_Y + ATTENDANCE_CONFIRM_H);
}

// Back button area on attendance screen (top-right)
bool IsInAttendanceBackButton(float x, float y) {
    return (x >= ATTENDANCE_BACK_X && x <= ATTENDANCE_BACK_X + ATTENDANCE_BACK_W &&
            y >= ATTENDANCE_BACK_Y && y <= ATTENDANCE_BACK_Y + ATTENDANCE_BACK_H);
}

int ReadBacklightMax() {
#ifdef DESKTOP_SIM
    return BACKLIGHT_FULL_DEFAULT;
#else
    int fd = open(BACKLIGHT_MAX_PATH, O_RDONLY);
    if (fd < 0) return BACKLIGHT_FULL_DEFAULT;
    char buf[16] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return BACKLIGHT_FULL_DEFAULT;
    int value = atoi(buf);
    return value > 0 ? value : BACKLIGHT_FULL_DEFAULT;
#endif
}

bool WriteBacklightValue(int value) {
#ifdef DESKTOP_SIM
    (void)value;
    return true;
#else
    int fd = open(BACKLIGHT_BRIGHTNESS_PATH, O_WRONLY);
    if (fd < 0) return false;
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", value);
    ssize_t written = write(fd, buf, (size_t)len);
    close(fd);
    return written == len;
#endif
}

void DeviceDelayUs(unsigned int microseconds) {
#ifdef DESKTOP_SIM
    (void)microseconds;
#else
    usleep(microseconds);
#endif
}

void InitializeAPIBackend() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void CleanupAPIBackend() {
    curl_global_cleanup();
}

void ApplyBacklightValue(AppContext& ctx, int value) {
    int clamped = value;
    if (clamped < 0) clamped = 0;
    if (clamped > ctx.backlight_max) clamped = ctx.backlight_max;
    if (clamped == ctx.backlight_current) return;
    if (WriteBacklightValue(clamped)) {
        ctx.backlight_current = clamped;
    }
}

void SetBacklightImmediate(AppContext& ctx, int value) {
    int clamped = value;
    if (clamped < 0) clamped = 0;
    if (clamped > ctx.backlight_max) clamped = ctx.backlight_max;
    ctx.backlight_target = clamped;
    ctx.backlight_transition_start = (float)clamped;
    ctx.backlight_transition_t = 1.0f;
    ctx.backlight_current_f = (float)clamped;
    ApplyBacklightValue(ctx, clamped);
}

int GetFullBrightness(const AppContext& ctx) {
    int full_value = BACKLIGHT_FULL_DEFAULT;
    if (full_value > ctx.backlight_max) full_value = ctx.backlight_max;
    return full_value;
}

void MarkActivity(AppContext& ctx) {
    ctx.activity_detected = true;
}

void SetBacklightTarget(AppContext& ctx, int value) {
    int clamped = value;
    if (clamped < 0) clamped = 0;
    if (clamped > ctx.backlight_max) clamped = ctx.backlight_max;
    if (clamped == ctx.backlight_target) return;
    if (ctx.backlight_current_f < 0.0f) {
        ctx.backlight_current_f = (float)clamped;
    }
    ctx.backlight_transition_start = ctx.backlight_current_f;
    ctx.backlight_transition_t = 0.0f;
    ctx.backlight_target = clamped;
}

void UpdateBacklightTransition(AppContext& ctx, float delta_time) {
    if (ctx.backlight_target < 0) return;
    if (ctx.backlight_transition_t < 1.0f) {
        float step = delta_time / BACKLIGHT_TRANSITION_SECONDS;
        ctx.backlight_transition_t += step;
        if (ctx.backlight_transition_t > 1.0f) ctx.backlight_transition_t = 1.0f;
    }
    float t = ctx.backlight_transition_t;
    float current_f = ctx.backlight_transition_start +
                      (ctx.backlight_target - ctx.backlight_transition_start) * t;
    ctx.backlight_current_f = current_f;
    int current_i = (int)(current_f + 0.5f);
    ApplyBacklightValue(ctx, current_i);
}

void UpdateBacklightInactivity(AppContext& ctx, float delta_time) {
    if (ctx.activity_detected) {
        ctx.inactivity_timer = 0.0f;
        SetBacklightTarget(ctx, GetFullBrightness(ctx));
    } else {
        ctx.inactivity_timer += delta_time;
        if (ctx.inactivity_timer >= BACKLIGHT_OFF_SECONDS) {
            SetBacklightTarget(ctx, 0);
        } else if (ctx.inactivity_timer >= BACKLIGHT_DIM_SECONDS) {
            int dim_value = BACKLIGHT_DIM_VALUE;
            if (dim_value > ctx.backlight_max) dim_value = ctx.backlight_max;
            SetBacklightTarget(ctx, dim_value);
        }
    }

    UpdateBacklightTransition(ctx, delta_time);
}

// ====================================================
// STATE HANDLERS
// ====================================================


std::string SignatureToBase64PNG(const std::vector<std::vector<ImVec2>>& strokes, int width, int height) {
    // Signature area bounds from ui_renderer.h
    const float SIG_MIN_X = 50;
    const float SIG_MIN_Y = 150;
    const float SIG_WIDTH = 550;
    const float SIG_HEIGHT = 270;
    
    // Create SVG with normalized coordinates
    std::stringstream svg;
    svg << "<svg width=\"" << width << "\" height=\"" << height 
        << "\" xmlns=\"http://www.w3.org/2000/svg\">";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
    
    for (const auto& stroke : strokes) {
        if (stroke.size() < 2) continue;
        
        svg << "<polyline points=\"";
        for (size_t i = 0; i < stroke.size(); i++) {
            // Normalize coordinates relative to signature box
            float normalized_x = ((stroke[i].x - SIG_MIN_X) / SIG_WIDTH) * width;
            float normalized_y = ((stroke[i].y - SIG_MIN_Y) / SIG_HEIGHT) * height;
            
            // Clamp to bounds
            normalized_x = std::max(0.0f, std::min((float)width, normalized_x));
            normalized_y = std::max(0.0f, std::min((float)height, normalized_y));
            
            svg << normalized_x << "," << normalized_y;
            if (i < stroke.size() - 1) svg << " ";
        }
        svg << "\" stroke=\"black\" stroke-width=\"3\" fill=\"none\"/>";
    }
    
    svg << "</svg>";
    
    return svg.str();
}

void HandleWaitingCardState(AppContext& ctx, float delta_time) {
    // Check for admin button tap (top-right area = clear button area) using touch handler
    bool clear_pressed = false;
    bool submit_pressed = false;
    bool cancel_pressed = false;
    std::vector<std::vector<ImVec2>> dummy_strokes;
    std::vector<ImVec2> dummy_current;
    bool dummy_drawing = false;
    ctx.touch_handler.ProcessInput(dummy_strokes, dummy_current, dummy_drawing, clear_pressed, submit_pressed, cancel_pressed);

    if (ctx.touch_handler.IsTouching()) {
        MarkActivity(ctx);
    }
    
    if (clear_pressed) {
        MarkActivity(ctx);
        ctx.admin_password_buffer.clear();
        ctx.admin_submit_requested = false;
        ctx.admin_input_buf[0] = '\0';
        ctx.ChangeState(STATE_ADMIN_PASSWORD);
        return;
    }

    RFIDData rfid_data = ctx.rfid_reader.Poll();

    // If no valid data, use consecutive empty polls to detect card removal
    if (!rfid_data.valid) {
        if (ctx.rfid_card_present) {
            ctx.rfid_no_data_frames++;
            if (ctx.rfid_no_data_frames >= RFID_REMOVE_FRAMES) {
                ctx.rfid_card_present = false;
                ctx.last_processed_rfid_uid.clear();
                ctx.rfid_no_data_frames = 0;
            }
        }
        return;
    }

    // Valid data received; reset removal counter
    ctx.rfid_no_data_frames = 0;
    MarkActivity(ctx);

    // If the same card is still present, ignore it until removed
    if (ctx.rfid_card_present && rfid_data.uid == ctx.last_processed_rfid_uid) {
        return;
    }

    std::string effective_uid = NormalizeRFIDUID(rfid_data.uid);
    printf("RFID Card: %s\n", effective_uid.c_str());
    
    // Store this UID as processed and mark card as present
    ctx.last_processed_rfid_uid = effective_uid;
    ctx.rfid_card_present = true;
        
        // Send to API to check user
        ScanResponse response = ctx.api_client.SendScan(effective_uid);
        
        if (response.success) {
            ctx.user_name = response.user_name;
            ctx.user_department = response.user_department;
            ctx.action = response.action;
            ctx.message = response.message;
            ctx.pending_rfid_uid = effective_uid;
            
            printf("+ %s - %s\n", ctx.action.c_str(), ctx.user_name.c_str());
            
            if (ctx.action == "clock_in") {
                // Fetch attendance before signature
                std::vector<std::string> attendance_dates;
                bool attendance_ok = ctx.api_client.FetchAttendanceLast30Days(effective_uid, attendance_dates);
                ctx.attendance_dates = attendance_dates;
                ctx.attendance_fetch_failed = !attendance_ok;
                ctx.attendance_warning = attendance_ok ? "" : "Aanwezigheid niet beschikbaar";

                // Going to attendance screen - turn LED RED
                ctx.api_client.SendDirectCommand("buzz");
                DeviceDelayUs(200000);
                ctx.api_client.SendDirectCommand("red_on");
                DeviceDelayUs(200000);  // 200ms - increased from 50ms
                ctx.api_client.SendDirectCommand("green_off");
                ctx.ChangeState(STATE_ATTENDANCE);
            } else {
                // Clock out - keep LED GREEN, just buzz
                ctx.api_client.SendDirectCommand("beep");
                ctx.ChangeState(STATE_SUCCESS);
            }
        } else {
            ctx.message = response.message;
            ctx.ChangeState(STATE_ERROR);
        }
}

void HandleAttendanceState(AppContext& ctx, float delta_time) {
#ifdef DESKTOP_SIM
    ImGuiIO& io = ImGui::GetIO();
    bool is_touching = io.MouseDown[0] && io.MousePos.x >= 0.0f && io.MousePos.y >= 0.0f;
    ImVec2 touch_pos = io.MousePos;
    if (is_touching) MarkActivity(ctx);
#else
    // Touch handling similar to admin password screen
    int fd = -1;
    if (ctx.touch_handler.fd < 0) {
        fd = open(TOUCH_DEV_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0) return;
    } else {
        fd = ctx.touch_handler.fd;
    }

    struct input_event ev;
    bool is_touching = false;
    ImVec2 touch_pos = ImVec2(0, 0);

    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_ABS) {
            switch (ev.code) {
                case ABS_MT_SLOT:
                    ctx.attendance_touch.current_slot = ev.value;
                    if (ctx.attendance_touch.current_slot < 0 || ctx.attendance_touch.current_slot >= 10)
                        ctx.attendance_touch.current_slot = 0;
                    break;
                case ABS_MT_TRACKING_ID:
                    ctx.attendance_touch.slots[ctx.attendance_touch.current_slot].active = (ev.value >= 0);
                    break;
                case ABS_MT_POSITION_X:
                    ctx.attendance_touch.slots[ctx.attendance_touch.current_slot].y = ev.value;
                    break;
                case ABS_MT_POSITION_Y:
                    ctx.attendance_touch.slots[ctx.attendance_touch.current_slot].x = ev.value;
                    break;
            }
        }
    }

    int active_slot = -1;
    for (int i = 0; i < 10; ++i) {
        if (ctx.attendance_touch.slots[i].active) {
            active_slot = i;
            break;
        }
    }

    is_touching = (active_slot >= 0);
    if (is_touching) {
        MarkActivity(ctx);
        touch_pos.x = ctx.attendance_touch.slots[active_slot].x;
        touch_pos.y = ctx.attendance_touch.slots[active_slot].y;
    }
#endif

    const float table_min_x = 40.0f;
    const float table_max_x = 600.0f;
    const float table_min_y = 90.0f;
    const float table_max_y = 420.0f;
    const float table_content_top = 140.0f;
    const float row_h = 24.0f;
    const float visible_height = table_max_y - table_content_top;
    float total_height = row_h * (float)ctx.attendance_dates.size();
    float max_scroll = std::max(0.0f, total_height - visible_height);

    if (is_touching) {
        MarkActivity(ctx);
#ifdef DESKTOP_SIM
        ImVec2 normalized_pos = touch_pos;
#else
        ImVec2 normalized_pos = ImVec2(touch_pos.x, SCREEN_HEIGHT - touch_pos.y);
#endif
        if (!ctx.attendance_was_touching) {
            ctx.attendance_touch_start_pos = normalized_pos;
            ctx.attendance_last_touch_pos = normalized_pos;
            ctx.attendance_is_dragging = (normalized_pos.x >= table_min_x && normalized_pos.x <= table_max_x &&
                                          normalized_pos.y >= table_min_y && normalized_pos.y <= table_max_y);
            ctx.attendance_dragged = false;
        } else if (ctx.attendance_is_dragging) {
            float dy = normalized_pos.y - ctx.attendance_last_touch_pos.y;
            if (std::abs(dy) > 1.5f) {
                ctx.attendance_dragged = true;
            }
            ctx.attendance_scroll_offset -= dy;
            if (ctx.attendance_scroll_offset < -max_scroll) ctx.attendance_scroll_offset = -max_scroll;
            if (ctx.attendance_scroll_offset > 0.0f) ctx.attendance_scroll_offset = 0.0f;
            ctx.attendance_last_touch_pos = normalized_pos;
        }
    } else {
        if (ctx.attendance_was_touching) {
            ImVec2 start_pos = ctx.attendance_touch_start_pos;

            if (ctx.attendance_dragged) {
                ctx.attendance_is_dragging = false;
                ctx.attendance_was_touching = false;
                return;
            }

            if (IsInAttendanceBackButton(start_pos.x, start_pos.y)) {
                ctx.pending_rfid_uid.clear();
                ctx.Reset();
                ctx.ChangeState(STATE_WAITING_CARD);
                ctx.attendance_was_touching = false;
                return;
            }

            if (IsInAttendanceConfirmButton(start_pos.x, start_pos.y)) {
                ctx.ChangeState(STATE_SIGNATURE);
                ctx.attendance_was_touching = false;
                return;
            }
        }
    }

    ctx.attendance_was_touching = is_touching;
}

void HandleAdminPasswordState(AppContext& ctx, float delta_time) {
#ifdef DESKTOP_SIM
    ImGuiIO& io = ImGui::GetIO();
    bool is_touching = io.MouseDown[0] && io.MousePos.x >= 0.0f && io.MousePos.y >= 0.0f;
    ImVec2 touch_pos = io.MousePos;
#else
    // Use touch handler to poll touch events, similar to signature screen
    bool clear_pressed = false;
    bool submit_pressed = false;
    std::vector<std::vector<ImVec2>> dummy_strokes;
    std::vector<ImVec2> dummy_current;
    bool dummy_drawing = false;
    
    // Reuse touch handler's existing touch reading infrastructure
    // We'll track touch position ourselves for keypad detection
    int fd = -1;
    if (ctx.touch_handler.fd < 0) {
        fd = open(TOUCH_DEV_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0) return;
    } else {
        fd = ctx.touch_handler.fd;
    }
    
    struct input_event ev;
    bool is_touching = false;
    ImVec2 touch_pos = ImVec2(0, 0);
    
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_TRACKING_ID && ev.value >= 0) {
                is_touching = true;
            } else if (ev.code == ABS_MT_POSITION_X) {
                touch_pos.y = ev.value;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                touch_pos.x = ev.value;
            }
        }
    }
#endif
    
    // Normalize coordinates like touch handler does
    if (is_touching) {
        MarkActivity(ctx);
#ifdef DESKTOP_SIM
        ImVec2 normalized_pos = touch_pos;
#else
        ImVec2 normalized_pos = ImVec2(touch_pos.x, SCREEN_HEIGHT - touch_pos.y);
#endif
        
        if (!ctx.admin_was_touching) {
            ctx.admin_touch_start_pos = normalized_pos;
        }
    } else {
        // Touch released - check if it was on a button
        if (ctx.admin_was_touching) {
            ImVec2 start_pos = ctx.admin_touch_start_pos;
            
            // Check back button
            if (IsInPasswordBackButton(start_pos.x, start_pos.y)) {
                ctx.admin_password_buffer.clear();
                ctx.ChangeState(STATE_WAITING_CARD);
                ctx.admin_was_touching = false;
                return;
            }
            
            // Check keypad buttons (1-9)
            for (int digit = 1; digit <= 9; digit++) {
                if (IsInKeypadButton(start_pos.x, start_pos.y, digit)) {
                    if (ctx.admin_password_buffer.length() < 10) {
                        ctx.admin_password_buffer += std::to_string(digit);
                        printf("PIN: %s\n", ctx.admin_password_buffer.c_str());
                        ctx.admin_last_digit = digit;
                        ctx.admin_last_digit_time = (float)ImGui::GetTime();
                    }
                    
                    // Auto-validate when correct number of digits
                    if (ctx.admin_password_buffer.length() == ADMIN_PASSWORD.length()) {
                        if (ctx.admin_password_buffer == ADMIN_PASSWORD) {
                            ctx.admin_displayed_rfid.clear();
                            ctx.ChangeState(STATE_ADMIN);
                        } else {
                            ctx.message = "Pincode onjuist";
                            ctx.admin_password_buffer.clear();
                            ctx.ChangeState(STATE_ERROR);
                        }
                    }
                    ctx.admin_was_touching = false;
                    return;
                }
            }
        }
    }
    
    ctx.admin_was_touching = is_touching;
}

void HandleAdminState(AppContext& ctx, float delta_time) {
    // Poll RFID reader and show UID when card is presented
    RFIDData rfid_data = ctx.rfid_reader.Poll();
    if (rfid_data.valid) {
        MarkActivity(ctx);
        ctx.admin_displayed_rfid = rfid_data.uid;
        // Treat card as present so waiting screen doesn't auto-process while it's held
        ctx.last_processed_rfid_uid = rfid_data.uid;
        ctx.rfid_card_present = true;
        ctx.rfid_no_data_frames = 0;
    }

#ifdef DESKTOP_SIM
    ImGuiIO& io = ImGui::GetIO();
    bool is_touching = io.MouseDown[0] && io.MousePos.x >= 0.0f && io.MousePos.y >= 0.0f;
    ImVec2 touch_pos = io.MousePos;
#else
    // Detect back button using same pattern as admin password
    int fd = -1;
    if (ctx.touch_handler.fd < 0) {
        fd = open(TOUCH_DEV_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0) return;
    } else {
        fd = ctx.touch_handler.fd;
    }
    
    struct input_event ev;
    bool is_touching = false;
    ImVec2 touch_pos = ImVec2(0, 0);
    
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_TRACKING_ID && ev.value >= 0) {
                is_touching = true;
            } else if (ev.code == ABS_MT_POSITION_X) {
                touch_pos.y = ev.value;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                touch_pos.x = ev.value;
            }
        }
    }
#endif
    
    if (is_touching) {
        MarkActivity(ctx);
#ifdef DESKTOP_SIM
        ImVec2 normalized_pos = touch_pos;
#else
        ImVec2 normalized_pos = ImVec2(touch_pos.x, SCREEN_HEIGHT - touch_pos.y);
#endif
        if (!ctx.admin_was_touching) {
            ctx.admin_touch_start_pos = normalized_pos;
        }
    } else {
        if (ctx.admin_was_touching) {
            // Check back button
            if (IsInAdminBackButton(ctx.admin_touch_start_pos.x, ctx.admin_touch_start_pos.y)) {
                ctx.ChangeState(STATE_WAITING_CARD);
            }
        }
    }
    
    ctx.admin_was_touching = is_touching;
}

void HandleSignatureState(AppContext& ctx, float delta_time) {
    bool clear_pressed = false;
    bool submit_pressed = false;
    bool cancel_pressed = false;
    ctx.touch_handler.ProcessInput(
        ctx.signature_strokes, 
        ctx.current_stroke, 
        ctx.is_drawing, 
        clear_pressed,
        submit_pressed,
        cancel_pressed
    );

    if (ctx.touch_handler.IsTouching() || ctx.is_drawing || clear_pressed || submit_pressed || cancel_pressed) {
        MarkActivity(ctx);
    }
    
    if (clear_pressed) {
        ctx.ClearSignature();
        printf("+ Signature cleared\n");
    }
    
    if (submit_pressed) {
        if (!ctx.signature_strokes.empty()) {
            printf("Submitting signature...\n");
            
            // CRITICAL: Store the UID before sending, then clear pending_rfid_uid
            // to prevent any duplicate processing if the card is still in range
            std::string uid_to_send = ctx.pending_rfid_uid;
            ctx.pending_rfid_uid.clear();  // Clear immediately - this UID is now being processed
            
            // Convert signature to SVG string
            std::string signature_svg = SignatureToBase64PNG(ctx.signature_strokes, 550, 270);
            
            // Send to API
            if (ctx.api_client.SendClockInWithSignature(uid_to_send, signature_svg)) {
                printf("+ Clock-in with signature successful\n");
                
                // Success! Turn LED back to GREEN
                ctx.api_client.SendDirectCommand("buzz");
                DeviceDelayUs(200000);
                ctx.api_client.SendDirectCommand("red_off");
                DeviceDelayUs(200000);
                ctx.api_client.SendDirectCommand("green_on");
                
                ctx.ChangeState(STATE_SUCCESS);
            } else {
                printf("- Failed to submit signature\n");
                ctx.message = "Handtekening versturen mislukt";
                ctx.ChangeState(STATE_ERROR);
            }
        } else {
            printf("- No signature to submit\n");
            ctx.message = "Teken uw handtekening";
            ctx.ChangeState(STATE_ERROR);
        }
    }

    if (cancel_pressed) {
        printf("- Signature cancelled\n");
        ctx.pending_rfid_uid.clear();
        ctx.Reset();
        // Return to waiting state and restore LED
        ctx.api_client.SendDirectCommand("green_on");
        DeviceDelayUs(50000);
        ctx.api_client.SendDirectCommand("red_off");
        ctx.ChangeState(STATE_WAITING_CARD);
    }
}

void HandleSuccessState(AppContext& ctx, float delta_time) {
    ctx.state_timer += delta_time;
    
    if (ctx.state_timer >= ctx.message_duration) {
        // CRITICAL: Clear pending UID BEFORE returning to waiting state
        // This ensures no duplicate transactions if card is still in range
        ctx.pending_rfid_uid.clear();
        
        ctx.Reset();
        
        // Returning to waiting state - ensure GREEN is on
        ctx.api_client.SendDirectCommand("green_on");
        DeviceDelayUs(50000);
        ctx.api_client.SendDirectCommand("red_off");
        
        ctx.ChangeState(STATE_WAITING_CARD);
    }
}

void HandleErrorState(AppContext& ctx, float delta_time) {
    ctx.state_timer += delta_time;
    
    if (ctx.state_timer >= ctx.message_duration) {
        // CRITICAL: Clear pending UID BEFORE returning to waiting state
        ctx.pending_rfid_uid.clear();
        
        ctx.Reset();
        
        // Returning to waiting state - ensure GREEN is on
        ctx.api_client.SendDirectCommand("green_on");
        DeviceDelayUs(50000);
        ctx.api_client.SendDirectCommand("red_off");
        
        ctx.ChangeState(STATE_WAITING_CARD);
    }
}



// ====================================================
// MAIN LOOP
// ====================================================

void UpdateApp(AppContext& ctx, float delta_time) {
    ctx.activity_detected = false;

    // State transition
    if (ctx.next_state != ctx.current_state) {
        ctx.current_state = ctx.next_state;
    }
    
    // Handle current state
    switch (ctx.current_state) {
        case STATE_WAITING_CARD:
            HandleWaitingCardState(ctx, delta_time);
            break;

        case STATE_ATTENDANCE:
            HandleAttendanceState(ctx, delta_time);
            break;
            
        case STATE_SIGNATURE:
            HandleSignatureState(ctx, delta_time);
            break;
            
        case STATE_SUCCESS:
            HandleSuccessState(ctx, delta_time);
            break;
            
        case STATE_ERROR:
            HandleErrorState(ctx, delta_time);
            break;
            
        case STATE_ADMIN_PASSWORD:
            HandleAdminPasswordState(ctx, delta_time);
            break;

        case STATE_ADMIN:
            HandleAdminState(ctx, delta_time);
            break;
    }

    UpdateBacklightInactivity(ctx, delta_time);
}

void RenderApp(AppContext& ctx) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    
    switch (ctx.current_state) {
        case STATE_WAITING_CARD:
            ctx.ui_renderer.RenderWaitingScreen(draw_list);
            break;

        case STATE_ATTENDANCE:
            ctx.ui_renderer.RenderAttendanceScreen(
                draw_list,
                ctx.user_name,
                ctx.attendance_dates,
                ctx.attendance_warning,
                ctx.attendance_scroll_offset
            );
            break;
            
        case STATE_SIGNATURE:
            ctx.ui_renderer.RenderSignatureScreen(
                draw_list,
                ctx.user_name,
                ctx.signature_strokes,
                ctx.current_stroke
            );
            break;
            
        case STATE_SUCCESS:
            ctx.ui_renderer.RenderSuccessScreen(
                draw_list,
                ctx.user_name,
                ctx.action
            );
            break;
            
        case STATE_ERROR:
            ctx.ui_renderer.RenderErrorScreen(
                draw_list,
                ctx.message
            );
            break;

        case STATE_ADMIN_PASSWORD:
            ctx.ui_renderer.RenderAdminPasswordScreen(draw_list, ctx.admin_password_buffer, ctx.admin_last_digit, ctx.admin_last_digit_time);
            break;

        case STATE_ADMIN:
            ctx.ui_renderer.RenderAdminScreen(draw_list, ctx.admin_displayed_rfid);
            break;
    }
}

#ifdef DESKTOP_SIM
static constexpr const char* DEFAULT_SIM_RFID_UID = "11F3EF12";
static bool g_show_simulator_help = true;
static std::string g_last_simulated_card = "none";

const char* GetStateName(AppState state) {
    switch (state) {
        case STATE_WAITING_CARD: return "waiting for card";
        case STATE_ATTENDANCE: return "attendance";
        case STATE_SIGNATURE: return "signature";
        case STATE_SUCCESS: return "success";
        case STATE_ERROR: return "error";
        case STATE_ADMIN_PASSWORD: return "admin PIN";
        case STATE_ADMIN: return "admin";
    }
    return "unknown";
}

void ResetSimulationFlow(AppContext& ctx) {
    ctx.pending_rfid_uid.clear();
    ctx.Reset();
    ctx.rfid_card_present = false;
    ctx.rfid_no_data_frames = 0;
    ctx.last_processed_rfid_uid.clear();
    ctx.rfid_reader.Flush();
    ctx.current_state = STATE_WAITING_CARD;
    ctx.next_state = STATE_WAITING_CARD;
    ctx.state_timer = 0.0f;
}

void InjectSimulationCard(AppContext& ctx, const std::string& uid, bool reset_flow) {
    if (uid.empty()) {
        printf("- [simulator] no RFID UID configured; use --sim-rfid=UID or STM32_SIM_RFID_UID\n");
        return;
    }
    if (reset_flow) ResetSimulationFlow(ctx);
    ctx.rfid_reader.InjectCard(uid);
    g_last_simulated_card = uid;
    printf("+ [simulator] card injected: %s\n", uid.c_str());
}

std::string GetConfiguredSimulationUID(const AppContext& ctx, const char* mock_uid) {
    const char* configured = std::getenv("STM32_SIM_RFID_UID");
    if (configured != nullptr && configured[0] != '\0') return configured;
    return ctx.api_client.IsMockMode() ? mock_uid : DEFAULT_SIM_RFID_UID;
}

void HandleSimulatorHotkeys(AppContext& ctx) {
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        InjectSimulationCard(ctx, GetConfiguredSimulationUID(ctx, "SIM_CLOCK_IN"), true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        InjectSimulationCard(ctx, GetConfiguredSimulationUID(ctx, "SIM_CLOCK_OUT"), true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
        InjectSimulationCard(ctx, "SIM_UNKNOWN", true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F4, false)) {
        InjectSimulationCard(ctx, GetConfiguredSimulationUID(ctx, "SIM_ADMIN_CARD"), false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        ResetSimulationFlow(ctx);
        g_last_simulated_card = "none";
        printf("+ [simulator] flow reset\n");
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) {
        g_show_simulator_help = !g_show_simulator_help;
    }
}

void RenderSimulatorHelp(const AppContext& ctx) {
    if (!g_show_simulator_help) return;

    char state_line[160];
    char card_line[192];
    snprintf(state_line, sizeof(state_line), "WSL simulator | API: %s | state: %s", ctx.api_client.IsMockMode() ? "MOCK" : "LIVE", GetStateName(ctx.current_state));
    snprintf(card_line, sizeof(card_line), "Mouse = touch | last card: %s", g_last_simulated_card.c_str());

    ImDrawList* overlay = ImGui::GetForegroundDrawList();
    overlay->AddRectFilled(ImVec2(8.0f, 8.0f), ImVec2(405.0f, 112.0f), IM_COL32(20, 20, 20, 225), 5.0f);
    overlay->AddRect(ImVec2(8.0f, 8.0f), ImVec2(405.0f, 112.0f), IM_COL32(90, 90, 90, 255), 5.0f);
    const ImU32 text_color = IM_COL32(255, 255, 255, 255);
    overlay->AddText(ImVec2(16.0f, 14.0f), text_color, state_line);
    overlay->AddText(ImVec2(16.0f, 38.0f), text_color, ctx.api_client.IsMockMode() ? "F1 clock-in  F2 clock-out  F3 unknown card" : "F1 scan 11F3EF12  F2 same card  F3 unknown");
    overlay->AddText(ImVec2(16.0f, 62.0f), text_color, "F4 inject admin card  F5 reset  F12 hide help");
    overlay->AddText(ImVec2(16.0f, 86.0f), text_color, card_line);
}
#endif

// ====================================================
// MAIN
// ====================================================

int main(int argc, char** argv) {
    // Initialize CURL
    InitializeAPIBackend();
    
    printf("================================================\n");
    printf("RFID Attendance System\n");
#ifdef DESKTOP_SIM
    printf("WSL desktop hardware simulation\n");
#endif
    printf("================================================\n");
    
    // Initialize context
    AppContext ctx;
    ctx.backlight_max = ReadBacklightMax();
    SetBacklightImmediate(ctx, GetFullBrightness(ctx));
    
    // Initialize RFID reader FIRST
    if (!ctx.rfid_reader.Open()) {
        printf("- Failed to open RFID reader!\n");
        CleanupAPIBackend();
        return 1;
    }
    
    // NOW connect the API client to the RFID reader
    ctx.api_client.SetRFIDReader(&ctx.rfid_reader);

    // Test API connection
    if (!ctx.api_client.TestConnection()) {
        printf("- Warning: API connection failed. System may not work properly.\n");
    }
    


    
    // Initialize GLFW
    if (!glfwInit()) {
        printf("- Failed to initialize GLFW!\n");
        CleanupAPIBackend();
        return 1;
    }
    
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    
#ifdef DESKTOP_SIM
    GLFWmonitor* target_monitor = NULL;
    const char* window_title = "RFID Attendance - WSL Simulator";
#else
    GLFWmonitor* target_monitor = glfwGetPrimaryMonitor();
    const char* window_title = "RFID Attendance";
#endif
    GLFWwindow* window = glfwCreateWindow(800, 480, window_title, target_monitor, NULL);
    if (!window) {
        printf("- Failed to create window!\n");
        glfwTerminate();
        CleanupAPIBackend();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
#ifdef DESKTOP_SIM
    io.MouseDrawCursor = true;
#else
    io.MouseDrawCursor = false;
#endif

    ImFont* jakarta_font = LoadJakartaSans(io, 20.0f);
    if (jakarta_font != nullptr) {
        io.FontDefault = jakarta_font;
    }
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    printf("+ System ready\n");
#ifdef DESKTOP_SIM
    printf("+ Simulator API mode: %s\n", ctx.api_client.IsMockMode() ? "MOCK" : "LIVE");
    printf("+ Simulator keys: F1/F2 scan configured card, F3 unknown, F4 admin card, F5 reset, F12 help\n");
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const std::string prefix = "--sim-rfid=";
        if (argument.compare(0, prefix.size(), prefix) == 0) {
            InjectSimulationCard(ctx, argument.substr(prefix.size()), true);
        }
    }
#else
    (void)argc;
    (void)argv;
#endif
    printf("================================================\n\n");
    
    // Main loop
    float last_time = glfwGetTime();
// default is green
    ctx.api_client.SendDirectCommand("green_on");
    DeviceDelayUs(50000);
    ctx.api_client.SendDirectCommand("red_off");


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Calculate delta time
        float current_time = glfwGetTime();
        float delta_time = current_time - last_time;
        last_time = current_time;
        
        // Start the ImGui frame before processing input.  This makes mouse
        // events deterministic in the simulator and is also valid on target.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

#ifdef DESKTOP_SIM
        HandleSimulatorHotkeys(ctx);
#endif

        // Update application logic
        UpdateApp(ctx, delta_time);
        
        RenderApp(ctx);
#ifdef DESKTOP_SIM
        RenderSimulatorHelp(ctx);
#endif
        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        // A previous clipped draw must never restrict the next framebuffer
        // clear; otherwise stale black rectangles can survive a state change.
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    ctx.rfid_reader.Close();
    CleanupAPIBackend();
    
    return 0;
}
