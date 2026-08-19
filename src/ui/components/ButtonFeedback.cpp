#include "ui/components/ButtonFeedback.hpp"

namespace clockin {

void ButtonFeedback::Press(UiControl control) {
    pressed_ = control;
    released_ = UiControl::None;
}

void ButtonFeedback::CancelPress() { pressed_ = UiControl::None; }

void ButtonFeedback::Release(UiControl control, double now_seconds) {
    pressed_ = UiControl::None;
    released_ = control;
    released_at_ = now_seconds;
}

void ButtonFeedback::Clear() {
    pressed_ = UiControl::None;
    released_ = UiControl::None;
    released_at_ = 0.0;
}

bool ButtonFeedback::IsPressed(UiControl control) const {
    return control != UiControl::None && pressed_ == control;
}

bool ButtonFeedback::IsAnimating(UiControl control, double now_seconds) const {
    return control != UiControl::None && released_ == control &&
           now_seconds >= released_at_ && now_seconds - released_at_ < kReleaseSeconds;
}

float ButtonFeedback::Scale(UiControl control, double now_seconds) const {
    if (IsPressed(control)) return 0.97f;
    if (!IsAnimating(control, now_seconds)) return 1.0f;
    const float remaining =
        static_cast<float>(1.0 - (now_seconds - released_at_) / kReleaseSeconds);
    return 1.0f + 0.035f * remaining * remaining;
}

}  // namespace clockin
