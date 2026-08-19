#pragma once

#include "imgui.h"

#include <linux/input.h>
#include <string>
#include <vector>

namespace clockin {

enum class TouchEventType { Down, Move, Up, Cancel };

struct TouchEvent {
    TouchEventType type;
    ImVec2 position;
    double timestamp;
};

class TouchInput {
public:
    explicit TouchInput(std::string device_path);
    ~TouchInput();

    TouchInput(const TouchInput&) = delete;
    TouchInput& operator=(const TouchInput&) = delete;

    void Update(double now_seconds);
    const std::vector<TouchEvent>& Events() const { return events_; }
    bool IsTouching() const { return pointer_active_; }
    bool IsOpen() const { return connected_; }
    const ImVec2& Position() const { return last_position_; }
    unsigned int ReconnectCount() const { return reconnect_count_; }
    const std::string& DevicePath() const { return device_path_; }
    double LastSampleLatencyMs(double now_seconds) const;
    void SuppressUntilRelease();

    void FeedEventForTest(const input_event& event, double now_seconds);
    void ClearEventsForTest();
    void SetAxisRangesForTest(int x_min, int x_max, int y_min, int y_max);

private:
    static constexpr int kScreenWidth = 800;
    static constexpr int kScreenHeight = 480;
    static constexpr int kMaxSlots = 10;
    static constexpr std::size_t kMaxEventsPerFrame = 256;

    struct Slot {
        int raw_x = 0;
        int raw_y = 0;
        int tracking_id = -1;
        bool active = false;
        bool have_x = false;
        bool have_y = false;
    };

    int descriptor_ = -1;
    std::string device_path_;
    Slot slots_[kMaxSlots];
    int current_slot_ = 0;
    int primary_slot_ = -1;
    bool pointer_active_ = false;
    bool suppress_until_release_ = false;
    bool dropped_ = false;
    bool single_touch_active_ = false;
    int raw_x_min_ = 0;
    int raw_x_max_ = kScreenHeight;
    int raw_y_min_ = 0;
    int raw_y_max_ = kScreenWidth;
    double next_open_time_ = 0.0;
    double open_backoff_ = 0.25;
    bool unavailable_reported_ = false;
    bool connected_ = false;
    bool ever_connected_ = false;
    unsigned int reconnect_count_ = 0;
    bool kernel_clock_monotonic_ = false;
    double last_sample_timestamp_ = -1.0;
    ImVec2 last_position_{0.0f, 0.0f};
    std::vector<TouchEvent> events_;

    static float DistanceSquared(const ImVec2& first, const ImVec2& second);
    void ResetSlots();
    bool Open(double now_seconds);
    bool QueryAxisRange(unsigned int code, int& minimum, int& maximum);
    void Close();
    void CloseAfterFailure(double now_seconds);
    double EventTimestamp(const input_event& event, double fallback) const;
    ImVec2 Transform(int raw_x, int raw_y) const;
    void Emit(TouchEventType type, const ImVec2& position, double timestamp);
    void CancelPointer(double timestamp);
    void CommitReport(double timestamp);
    void ProcessEvent(const input_event& event, double now_seconds);
};

}  // namespace clockin
