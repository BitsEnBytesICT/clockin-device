#include "services/input/TouchInput.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <utility>

namespace clockin {

TouchInput::TouchInput(std::string device_path) : device_path_(std::move(device_path)) {
    ResetSlots();
    events_.reserve(kMaxEventsPerFrame);
}

TouchInput::~TouchInput() { Close(); }

void TouchInput::Update(double now_seconds) {
    events_.clear();
    last_sample_timestamp_ = -1.0;
    if (!Open(now_seconds)) return;
    // evdev always returns whole events, so read them in batches: one syscall
    // per frame while idle instead of one per 24-byte event while touching.
    constexpr std::size_t kBatch = 64;
    input_event batch[kBatch];
    std::size_t event_budget = 512;
    while (event_budget > 0) {
        const ssize_t count = read(descriptor_, batch, sizeof(batch));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0 || count % static_cast<ssize_t>(sizeof(input_event)) != 0) {
            if (count > 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                CancelPointer(now_seconds);
                CloseAfterFailure(now_seconds);
            }
            break;
        }
        const std::size_t events =
            std::min(event_budget, static_cast<std::size_t>(count) / sizeof(input_event));
        for (std::size_t index = 0; index < events; ++index) {
            ProcessEvent(batch[index], now_seconds);
        }
        event_budget -= events;
        if (static_cast<std::size_t>(count) < sizeof(batch)) break;
    }
}

double TouchInput::LastSampleLatencyMs(double now_seconds) const {
    if (last_sample_timestamp_ < 0.0) return -1.0;
    const double latency = (now_seconds - last_sample_timestamp_) * 1000.0;
    return latency >= 0.0 && latency < 10000.0 ? latency : -1.0;
}

void TouchInput::SuppressUntilRelease() {
    events_.clear();
    suppress_until_release_ = pointer_active_;
}

void TouchInput::FeedEventForTest(const input_event& event, double now_seconds) {
    ProcessEvent(event, now_seconds);
}

void TouchInput::ClearEventsForTest() { events_.clear(); }

void TouchInput::SetAxisRangesForTest(int x_min, int x_max, int y_min, int y_max) {
    raw_x_min_ = x_min;
    raw_x_max_ = x_max;
    raw_y_min_ = y_min;
    raw_y_max_ = y_max;
}

float TouchInput::DistanceSquared(const ImVec2& first, const ImVec2& second) {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    return dx * dx + dy * dy;
}

void TouchInput::ResetSlots() {
    for (Slot& slot : slots_) slot = Slot{};
    current_slot_ = 0;
    primary_slot_ = -1;
    single_touch_active_ = false;
}

bool TouchInput::Open(double now_seconds) {
    if (descriptor_ >= 0) return true;
    if (now_seconds < next_open_time_) return false;
    descriptor_ = open(device_path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor_ < 0) {
        if (!unavailable_reported_) {
            std::fprintf(stderr, "Touch device unavailable (%s): %s\n",
                         device_path_.c_str(), std::strerror(errno));
            unavailable_reported_ = true;
        }
        next_open_time_ = now_seconds + open_backoff_;
        open_backoff_ = std::min(2.0, open_backoff_ * 2.0);
        return false;
    }
    if (!QueryAxisRange(ABS_MT_POSITION_X, raw_x_min_, raw_x_max_)) {
        QueryAxisRange(ABS_X, raw_x_min_, raw_x_max_);
    }
    if (!QueryAxisRange(ABS_MT_POSITION_Y, raw_y_min_, raw_y_max_)) {
        QueryAxisRange(ABS_Y, raw_y_min_, raw_y_max_);
    }
    int clock_id = CLOCK_MONOTONIC;
    kernel_clock_monotonic_ = ioctl(descriptor_, EVIOCSCLOCKID, &clock_id) == 0;
    if (ever_connected_) ++reconnect_count_;
    ever_connected_ = true;
    connected_ = true;
    unavailable_reported_ = false;
    open_backoff_ = 0.25;
    ResetSlots();
    std::printf("Touch device ready: %s\n", device_path_.c_str());
    return true;
}

bool TouchInput::QueryAxisRange(unsigned int code, int& minimum, int& maximum) {
    input_absinfo info{};
    if (ioctl(descriptor_, EVIOCGABS(code), &info) != 0 || info.maximum <= info.minimum) {
        return false;
    }
    minimum = info.minimum;
    maximum = info.maximum;
    return true;
}

void TouchInput::Close() {
    if (descriptor_ >= 0) {
        close(descriptor_);
        descriptor_ = -1;
    }
    connected_ = false;
}

void TouchInput::CloseAfterFailure(double now_seconds) {
    Close();
    ResetSlots();
    next_open_time_ = now_seconds + open_backoff_;
    open_backoff_ = std::min(2.0, open_backoff_ * 2.0);
}

double TouchInput::EventTimestamp(const input_event& event, double fallback) const {
    if (!kernel_clock_monotonic_) return fallback;
    return static_cast<double>(event.input_event_sec) +
           static_cast<double>(event.input_event_usec) / 1000000.0;
}

ImVec2 TouchInput::Transform(int raw_x, int raw_y) const {
    const float x_range = static_cast<float>(raw_y_max_ - raw_y_min_);
    const float y_range = static_cast<float>(raw_x_max_ - raw_x_min_);
    float x = x_range > 0.0f
                  ? kScreenWidth -
                        (static_cast<float>(raw_y - raw_y_min_) / x_range) * kScreenWidth
                  : kScreenWidth - static_cast<float>(raw_y);
    float y = y_range > 0.0f
                  ? (static_cast<float>(raw_x - raw_x_min_) / y_range) * kScreenHeight
                  : static_cast<float>(raw_x);
    x = std::max(0.0f, std::min(static_cast<float>(kScreenWidth), x));
    y = std::max(0.0f, std::min(static_cast<float>(kScreenHeight), y));
    return {x, y};
}

void TouchInput::Emit(TouchEventType type, const ImVec2& position, double timestamp) {
    last_position_ = position;
    last_sample_timestamp_ = timestamp;
    if (suppress_until_release_) {
        if (type == TouchEventType::Up || type == TouchEventType::Cancel) {
            suppress_until_release_ = false;
        }
        return;
    }
    if (events_.size() >= kMaxEventsPerFrame) {
        events_.clear();
        events_.push_back({TouchEventType::Cancel, position, timestamp});
        suppress_until_release_ = pointer_active_;
        return;
    }
    events_.push_back({type, position, timestamp});
}

void TouchInput::CancelPointer(double timestamp) {
    if (pointer_active_) Emit(TouchEventType::Cancel, last_position_, timestamp);
    pointer_active_ = false;
    primary_slot_ = -1;
}

void TouchInput::CommitReport(double timestamp) {
    if (dropped_) {
        CancelPointer(timestamp);
        ResetSlots();
        dropped_ = false;
        return;
    }
    int active_slot = -1;
    for (int index = 0; index < kMaxSlots; ++index) {
        if (slots_[index].active && slots_[index].have_x && slots_[index].have_y) {
            active_slot = index;
            break;
        }
    }
    if (active_slot < 0 && single_touch_active_ && slots_[0].have_x && slots_[0].have_y) {
        active_slot = 0;
    }
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

void TouchInput::ProcessEvent(const input_event& event, double now_seconds) {
    const double timestamp = EventTimestamp(event, now_seconds);
    if (event.type == EV_SYN) {
        if (event.code == SYN_DROPPED) dropped_ = true;
        else if (event.code == SYN_REPORT) CommitReport(timestamp);
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
        current_slot_ = event.value >= 0 && event.value < kMaxSlots ? event.value : 0;
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

}  // namespace clockin
