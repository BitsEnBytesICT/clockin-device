#include "pages/AdminPinPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/PinKeypad.hpp"
#include "ui/components/VectorIcons.hpp"

namespace clockin {
namespace {

constexpr PinKeypadBounds kKeypad{{220.0f, 178.0f}, {580.0f, 452.0f}};

}  // namespace

UiControl AdminPinPage::HitTest(const ImVec2& point) const {
    if (PointInRectangle(point, 650, 10, 140, 60)) return UiControl::AdminBack;
    return PinKeypad::HitTest(point, kKeypad);
}

void AdminPinPage::Reset() {
    value_.clear();
    last_key_ = UiControl::None;
    last_key_time_ = 0.0;
}

void AdminPinPage::AppendDigit(int digit, double ui_time) {
    if (digit < 0 || digit > 9) return;
    ApplyKey(digit == 0
                 ? UiControl::Key0
                 : static_cast<UiControl>(static_cast<int>(UiControl::Key1) + digit - 1),
             ui_time);
}

void AdminPinPage::ApplyKey(UiControl control, double ui_time) {
    if (!PinKeypad::Apply(control, value_)) return;
    last_key_ = control;
    last_key_time_ = ui_time;
}

void AdminPinPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {40, 40}, {760, 465}, 14.0f, true);
    drawing.CenteredText(&draw, drawing.SemiBold(), 28.0f, 62.0f, theme::Text,
                         "Voer beheerders-pincode in");
    drawing.CenteredText(&draw, drawing.Regular(), 17.0f, 102.0f, theme::MutedText,
                         "Vier cijfers");
    const float dot_start = 400.0f - 3.0f * 17.0f;
    for (int index = 0; index < 4; ++index) {
        const ImVec2 center{dot_start + index * 34.0f, 143.0f};
        if (index < static_cast<int>(value_.size())) {
            draw.AddCircleFilled(center, 6.0f, theme::Blue, 20);
        } else {
            draw.AddCircle(center, 6.0f, theme::LightBlue, 20, 2.0f);
        }
    }
    PinKeypad::Render(draw, drawing, kKeypad, last_key_, last_key_time_);
    drawing.ActionButton(&draw, {650, 10}, {790, 70}, "Terug",
                         UiControl::AdminBack, ButtonVariant::Secondary,
                         VectorIcon::Back);
}

}  // namespace clockin
