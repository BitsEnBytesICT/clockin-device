#pragma once

#include "app/AppEvent.hpp"
#include "services/input/TouchInput.hpp"
#include "ui/components/UiControl.hpp"

#include <functional>
#include <vector>

namespace clockin {

class AttendancePage;
class ButtonFeedback;
class Page;
class SignaturePage;

class InputRouter {
public:
    using Activate = std::function<void(const AppEvent&)>;

    bool Process(const std::vector<TouchEvent>& events,
                 Page& active_page,
                 AttendancePage& attendance_page,
                 SignaturePage& signature_page,
                 ButtonFeedback& feedback,
                 double now_seconds,
                 const Activate& activate);
    void Reset();

private:
    struct Gesture {
        bool active = false;
        bool moved = false;
        UiControl target = UiControl::None;
        ImVec2 start{0.0f, 0.0f};
        ImVec2 last{0.0f, 0.0f};
    } gesture_;

    static float Distance(const ImVec2& first, const ImVec2& second);
};

}  // namespace clockin
