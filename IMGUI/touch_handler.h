#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

#include "imgui.h"

#include <cmath>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#define TOUCH_DEV_PATH "/dev/input/event1"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

enum class TouchEventType {
    Down,
    Move,
    Up,
    Cancel
};

struct TouchEvent {
    TouchEventType type;
    ImVec2 position;
    double timestamp;

    TouchEvent(TouchEventType event_type, const ImVec2& event_position, double event_timestamp)
        : type(event_type), position(event_position), timestamp(event_timestamp) {}
};

class TouchHandler {
public:
    TouchHandler()
        : fd_(-1),
          device_path_(ResolveDevicePath()),
          current_slot_(0),
          primary_slot_(-1),
          pointer_active_(false),
          suppress_until_release_(false),
          dropped_(false),
          single_touch_active_(false),
          raw_x_min_(0),
          raw_x_max_(SCREEN_HEIGHT),
          raw_y_min_(0),
          raw_y_max_(SCREEN_WIDTH),
          next_open_time_(0.0),
          open_backoff_(0.25),
          unavailable_reported_(false),
          connected_(false),
          ever_connected_(false),
          reconnect_count_(0),
          kernel_clock_monotonic_(false),
          last_sample_timestamp_(-1.0),
          last_position_(0.0f, 0.0f) {
        ResetSlots();
        events_.reserve(MAX_EVENTS_PER_FRAME);
    }

    ~TouchHandler() { Close(); }

    void Update(double now_seconds) {
        events_.clear();
        // A latency sample is only valid when an input change from this frame
        // is actually included in the frame that is about to be presented.
        last_sample_timestamp_ = -1.0;
#ifdef DESKTOP_SIM
        ImGuiIO& io = ImGui::GetIO();
        const bool valid_position = io.MousePos.x > -100000.0f && io.MousePos.y > -100000.0f;
        const bool down = io.MouseDown[0] && valid_position;
        const ImVec2 position = valid_position ? io.MousePos : last_position_;
        if (down && !pointer_active_) {
            pointer_active_ = true;
            primary_slot_ = 0;
            Emit(TouchEventType::Down, position, now_seconds);
        } else if (down && pointer_active_ && DistanceSquared(position, last_position_) > 0.01f) {
            Emit(TouchEventType::Move, position, now_seconds);
        } else if (!down && pointer_active_) {
            Emit(TouchEventType::Up, last_position_, now_seconds);
            pointer_active_ = false;
            primary_slot_ = -1;
        }
        if (down) last_position_ = position;
        connected_ = true;
#else
        if (!Open(now_seconds)) return;

        struct input_event event;
        size_t event_budget = 512;
        while (event_budget-- > 0) {
            const ssize_t count = read(fd_, &event, sizeof(event));
            if (count == static_cast<ssize_t>(sizeof(event))) {
                ProcessEvent(event, now_seconds);
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count > 0) {
                CancelPointer(now_seconds);
                CloseAfterFailure(now_seconds);
                break;
            }
            if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                CancelPointer(now_seconds);
                CloseAfterFailure(now_seconds);
            }
            break;
        }
#endif
    }

    const std::vector<TouchEvent>& Events() const { return events_; }
    bool IsTouching() const { return pointer_active_; }
    bool IsOpen() const { return connected_; }
    const ImVec2& Position() const { return last_position_; }
    unsigned int ReconnectCount() const { return reconnect_count_; }
    const std::string& DevicePath() const { return device_path_; }

    double LastSampleLatencyMs(double now_seconds) const {
        if (last_sample_timestamp_ < 0.0) return -1.0;
        const double latency = (now_seconds - last_sample_timestamp_) * 1000.0;
        return latency >= 0.0 && latency < 10000.0 ? latency : -1.0;
    }

    void SuppressUntilRelease() {
        events_.clear();
        suppress_until_release_ = pointer_active_;
    }

    void FeedEventForTest(const struct input_event& event, double now_seconds) {
        ProcessEvent(event, now_seconds);
    }

    void ClearEventsForTest() { events_.clear(); }

    void SetAxisRangesForTest(int x_min, int x_max, int y_min, int y_max) {
        raw_x_min_ = x_min;
        raw_x_max_ = x_max;
        raw_y_min_ = y_min;
        raw_y_max_ = y_max;
    }

private:
    static const int MAX_SLOTS = 10;
    static const size_t MAX_EVENTS_PER_FRAME = 256;

    struct Slot {
        int raw_x;
        int raw_y;
        int tracking_id;
        bool active;
        bool have_x;
        bool have_y;

        Slot()
            : raw_x(0), raw_y(0), tracking_id(-1), active(false), have_x(false), have_y(false) {}
    };

    int fd_;
    std::string device_path_;
    Slot slots_[MAX_SLOTS];
    int current_slot_;
    int primary_slot_;
    bool pointer_active_;
    bool suppress_until_release_;
    bool dropped_;
    bool single_touch_active_;
    int raw_x_min_;
    int raw_x_max_;
    int raw_y_min_;
    int raw_y_max_;
    double next_open_time_;
    double open_backoff_;
    bool unavailable_reported_;
    bool connected_;
    bool ever_connected_;
    unsigned int reconnect_count_;
    bool kernel_clock_monotonic_;
    double last_sample_timestamp_;
    ImVec2 last_position_;
    std::vector<TouchEvent> events_;

    static std::string ResolveDevicePath() {
        const char* configured = std::getenv("BITS_BYTES_TOUCH_DEVICE");
        return configured != NULL && configured[0] != '\0' ? configured : TOUCH_DEV_PATH;
    }

    static float DistanceSquared(const ImVec2& a, const ImVec2& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    void ResetSlots() {
        for (int i = 0; i < MAX_SLOTS; ++i) slots_[i] = Slot();
        current_slot_ = 0;
        primary_slot_ = -1;
        single_touch_active_ = false;
    }

    bool Open(double now_seconds) {
        if (fd_ >= 0) return true;
        if (now_seconds < next_open_time_) return false;

        fd_ = open(device_path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0) {
            if (!unavailable_reported_) {
                fprintf(stderr, "- Touch device unavailable (%s): %s\n", device_path_.c_str(), strerror(errno));
                unavailable_reported_ = true;
            }
            next_open_time_ = now_seconds + open_backoff_;
            open_backoff_ = open_backoff_ < 2.0 ? open_backoff_ * 2.0 : 2.0;
            if (open_backoff_ > 2.0) open_backoff_ = 2.0;
            return false;
        }

        if (!QueryAxisRange(ABS_MT_POSITION_X, raw_x_min_, raw_x_max_)) {
            QueryAxisRange(ABS_X, raw_x_min_, raw_x_max_);
        }
        if (!QueryAxisRange(ABS_MT_POSITION_Y, raw_y_min_, raw_y_max_)) {
            QueryAxisRange(ABS_Y, raw_y_min_, raw_y_max_);
        }
        int clock_id = CLOCK_MONOTONIC;
        kernel_clock_monotonic_ = ioctl(fd_, EVIOCSCLOCKID, &clock_id) == 0;
        if (ever_connected_) ++reconnect_count_;
        ever_connected_ = true;
        connected_ = true;
        unavailable_reported_ = false;
        open_backoff_ = 0.25;
        ResetSlots();
        printf("+ Touch device ready: %s\n", device_path_.c_str());
        return true;
    }

    bool QueryAxisRange(unsigned int code, int& minimum, int& maximum) {
        struct input_absinfo info;
        if (ioctl(fd_, EVIOCGABS(code), &info) == 0 && info.maximum > info.minimum) {
            minimum = info.minimum;
            maximum = info.maximum;
            return true;
        }
        return false;
    }

    void Close() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        connected_ = false;
    }

    void CloseAfterFailure(double now_seconds) {
        Close();
        ResetSlots();
        next_open_time_ = now_seconds + open_backoff_;
        open_backoff_ = open_backoff_ < 2.0 ? open_backoff_ * 2.0 : 2.0;
        if (open_backoff_ > 2.0) open_backoff_ = 2.0;
    }

    double EventTimestamp(const struct input_event& event, double fallback) const {
        if (!kernel_clock_monotonic_) return fallback;
        return static_cast<double>(event.input_event_sec) +
               static_cast<double>(event.input_event_usec) / 1000000.0;
    }

    ImVec2 Transform(int raw_x, int raw_y) const {
        const float x_range = static_cast<float>(raw_y_max_ - raw_y_min_);
        const float y_range = static_cast<float>(raw_x_max_ - raw_x_min_);
        float x = x_range > 0.0f
            ? (static_cast<float>(raw_y - raw_y_min_) / x_range) * SCREEN_WIDTH
            : static_cast<float>(raw_y);
        float y = y_range > 0.0f
            ? SCREEN_HEIGHT - (static_cast<float>(raw_x - raw_x_min_) / y_range) * SCREEN_HEIGHT
            : SCREEN_HEIGHT - static_cast<float>(raw_x);
        if (x < 0.0f) x = 0.0f;
        if (x > SCREEN_WIDTH) x = SCREEN_WIDTH;
        if (y < 0.0f) y = 0.0f;
        if (y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;
        return ImVec2(x, y);
    }

    void Emit(TouchEventType type, const ImVec2& position, double timestamp) {
        last_position_ = position;
        last_sample_timestamp_ = timestamp;
        if (suppress_until_release_) {
            if (type == TouchEventType::Up || type == TouchEventType::Cancel) suppress_until_release_ = false;
            return;
        }
        if (events_.size() >= MAX_EVENTS_PER_FRAME) {
            events_.clear();
            events_.push_back(TouchEvent(TouchEventType::Cancel, position, timestamp));
            suppress_until_release_ = pointer_active_;
            return;
        }
        events_.push_back(TouchEvent(type, position, timestamp));
    }

    void CancelPointer(double timestamp) {
        if (pointer_active_) Emit(TouchEventType::Cancel, last_position_, timestamp);
        pointer_active_ = false;
        primary_slot_ = -1;
    }

    void CommitReport(double timestamp) {
        if (dropped_) {
            CancelPointer(timestamp);
            ResetSlots();
            dropped_ = false;
            return;
        }

        int active_slot = -1;
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots_[i].active && slots_[i].have_x && slots_[i].have_y) {
                active_slot = i;
                break;
            }
        }

        if (active_slot < 0 && single_touch_active_ && slots_[0].have_x && slots_[0].have_y) active_slot = 0;

        if (active_slot < 0) {
            if (pointer_active_) Emit(TouchEventType::Up, last_position_, timestamp);
            pointer_active_ = false;
            primary_slot_ = -1;
            return;
        }

        const ImVec2 position = Transform(slots_[active_slot].raw_x, slots_[active_slot].raw_y);
        if (!pointer_active_) {
            pointer_active_ = true;
            primary_slot_ = active_slot;
            Emit(TouchEventType::Down, position, timestamp);
        } else if (active_slot != primary_slot_) {
            Emit(TouchEventType::Cancel, last_position_, timestamp);
            primary_slot_ = active_slot;
            Emit(TouchEventType::Down, position, timestamp);
        } else if (DistanceSquared(position, last_position_) > 0.01f) {
            Emit(TouchEventType::Move, position, timestamp);
        }
    }

    void ProcessEvent(const struct input_event& event, double now_seconds) {
        const double timestamp = EventTimestamp(event, now_seconds);
        if (event.type == EV_SYN) {
            if (event.code == SYN_DROPPED) {
                dropped_ = true;
            } else if (event.code == SYN_REPORT) {
                CommitReport(timestamp);
            }
            return;
        }
        if (dropped_) return;

        if (event.type == EV_KEY && event.code == BTN_TOUCH) {
            single_touch_active_ = event.value != 0;
            slots_[0].active = single_touch_active_;
            return;
        }
        if (event.type != EV_ABS) return;

        if (event.code == ABS_MT_SLOT) {
            current_slot_ = event.value >= 0 && event.value < MAX_SLOTS ? event.value : 0;
        } else if (event.code == ABS_MT_TRACKING_ID) {
            Slot& slot = slots_[current_slot_];
            slot.tracking_id = event.value;
            slot.active = event.value >= 0;
            if (!slot.active) {
                slot.have_x = false;
                slot.have_y = false;
            }
        } else if (event.code == ABS_MT_POSITION_X || event.code == ABS_X) {
            Slot& slot = event.code == ABS_X ? slots_[0] : slots_[current_slot_];
            slot.raw_x = event.value;
            slot.have_x = true;
        } else if (event.code == ABS_MT_POSITION_Y || event.code == ABS_Y) {
            Slot& slot = event.code == ABS_Y ? slots_[0] : slots_[current_slot_];
            slot.raw_y = event.value;
            slot.have_y = true;
        }
    }
};

#endif
