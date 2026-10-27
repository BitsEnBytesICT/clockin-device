#pragma once

#include "ui/components/UiControl.hpp"

namespace clockin {

class ButtonFeedback {
public:
    void Press(UiControl control, double now_seconds);
    void CancelPress(double now_seconds);
    void Release(UiControl control, double now_seconds);
    void Clear();
    bool IsPressed(UiControl control) const;
    bool IsAnimating(UiControl control, double now_seconds) const;
    float Scale(UiControl control, double now_seconds) const;

private:
    static constexpr double kPressSeconds = 0.07;
    static constexpr double kReleaseSeconds = 0.22;
    UiControl pressed_ = UiControl::None;
    UiControl released_ = UiControl::None;
    double pressed_at_ = 0.0;
    double released_at_ = 0.0;
    float released_from_ = 1.0f;
    bool release_bounces_ = false;

    float PressedScale(double now_seconds) const;
    void BeginRelease(double now_seconds, bool bounce);
};

}  // namespace clockin
