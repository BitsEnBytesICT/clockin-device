#ifndef BITS_BYTES_UI_FEEDBACK_H
#define BITS_BYTES_UI_FEEDBACK_H

enum class UIControl {
    None,
    Admin,
    AttendanceBack,
    AttendanceConfirm,
    SignatureClear,
    SignatureCancel,
    SignatureSubmit,
    AdminBack,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
    Key6,
    Key7,
    Key8,
    Key9
};

// Visual feedback only. Hit testing and activation remain owned by main.cpp.
// Keeping the animation state here makes it deterministic and directly testable.
class UIButtonFeedback {
public:
    UIButtonFeedback()
        : pressed_(UIControl::None), released_(UIControl::None), released_at_(0.0) {}

    void Press(UIControl control) {
        pressed_ = control;
        released_ = UIControl::None;
    }

    void CancelPress() {
        pressed_ = UIControl::None;
    }

    void Release(UIControl control, double now_seconds) {
        pressed_ = UIControl::None;
        released_ = control;
        released_at_ = now_seconds;
    }

    void Clear() {
        pressed_ = UIControl::None;
        released_ = UIControl::None;
        released_at_ = 0.0;
    }

    bool IsPressed(UIControl control) const {
        return control != UIControl::None && pressed_ == control;
    }

    bool IsAnimating(UIControl control, double now_seconds) const {
        return control != UIControl::None && released_ == control &&
               now_seconds >= released_at_ && now_seconds - released_at_ < ReleaseSeconds();
    }

    float Scale(UIControl control, double now_seconds) const {
        if (IsPressed(control)) return 0.97f;
        if (!IsAnimating(control, now_seconds)) return 1.0f;
        const float remaining = static_cast<float>(
            1.0 - (now_seconds - released_at_) / ReleaseSeconds());
        return 1.0f + 0.035f * remaining * remaining;
    }

private:
    static double ReleaseSeconds() { return 0.18; }

    UIControl pressed_;
    UIControl released_;
    double released_at_;
};

#endif
