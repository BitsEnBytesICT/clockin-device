#pragma once

#include "ui/components/UiControl.hpp"

namespace clockin {

class ButtonFeedback {
public:
    void Press(UiControl control);
    void CancelPress();
    void Release(UiControl control, double now_seconds);
    void Clear();
    bool IsPressed(UiControl control) const;
    bool IsAnimating(UiControl control, double now_seconds) const;
    float Scale(UiControl control, double now_seconds) const;

private:
    static constexpr double kReleaseSeconds = 0.18;
    UiControl pressed_ = UiControl::None;
    UiControl released_ = UiControl::None;
    double released_at_ = 0.0;
};

}  // namespace clockin
