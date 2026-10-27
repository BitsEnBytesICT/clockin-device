#include "ui/components/ButtonFeedback.hpp"

#include <algorithm>
#include <cmath>

namespace clockin {
namespace {

constexpr float kPressedScale = 0.97f;
constexpr float kBounceAmount = 0.06f;
constexpr float kPi = 3.14159265f;

float Clamp01(double value) {
    return static_cast<float>(std::max(0.0, std::min(1.0, value)));
}

}  // namespace

void ButtonFeedback::Press(UiControl control, double now_seconds) {
    pressed_ = control;
    pressed_at_ = now_seconds;
    released_ = UiControl::None;
}

void ButtonFeedback::CancelPress(double now_seconds) {
    if (pressed_ == UiControl::None) return;
    BeginRelease(now_seconds, false);
}

void ButtonFeedback::Release(UiControl control, double now_seconds) {
    if (pressed_ != control) {
        pressed_ = control;
        pressed_at_ = now_seconds - kPressSeconds;
    }
    BeginRelease(now_seconds, true);
}

void ButtonFeedback::Clear() {
    pressed_ = UiControl::None;
    released_ = UiControl::None;
    pressed_at_ = 0.0;
    released_at_ = 0.0;
    released_from_ = 1.0f;
    release_bounces_ = false;
}

bool ButtonFeedback::IsPressed(UiControl control) const {
    return control != UiControl::None && pressed_ == control;
}

bool ButtonFeedback::IsAnimating(UiControl control, double now_seconds) const {
    return control != UiControl::None && released_ == control && release_bounces_ &&
           now_seconds >= released_at_ && now_seconds - released_at_ < kReleaseSeconds;
}

float ButtonFeedback::Scale(UiControl control, double now_seconds) const {
    if (IsPressed(control)) return PressedScale(now_seconds);
    if (control == UiControl::None || released_ != control ||
        now_seconds < released_at_ || now_seconds - released_at_ >= kReleaseSeconds) {
        return 1.0f;
    }
    // Starts exactly at the scale the press reached, so neither the press nor
    // the release ever jumps between frames; a confirmed tap adds a soft bounce.
    const float t = Clamp01((now_seconds - released_at_) / kReleaseSeconds);
    const float remaining = 1.0f - t;
    const float settle = (released_from_ - 1.0f) * remaining * remaining * remaining;
    const float bounce = release_bounces_
                             ? kBounceAmount * std::sin(kPi * t) * remaining
                             : 0.0f;
    return 1.0f + settle + bounce;
}

float ButtonFeedback::PressedScale(double now_seconds) const {
    const float progress = Clamp01((now_seconds - pressed_at_) / kPressSeconds);
    const float inverse = 1.0f - progress;
    const float eased = 1.0f - inverse * inverse * inverse;
    return 1.0f + (kPressedScale - 1.0f) * eased;
}

void ButtonFeedback::BeginRelease(double now_seconds, bool bounce) {
    released_from_ = PressedScale(now_seconds);
    released_ = pressed_;
    released_at_ = now_seconds;
    release_bounces_ = bounce;
    pressed_ = UiControl::None;
}

}  // namespace clockin
