#include "pages/AdminPinPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <cfloat>

namespace clockin {
namespace {
constexpr float kButtonWidth = 120.0f;
constexpr float kButtonHeight = 80.0f;
constexpr float kStartX = 205.0f;
constexpr float kStartY = 190.0f;
constexpr float kSpacingX = 115.0f;
constexpr float kSpacingY = 90.0f;
}

UiControl AdminPinPage::HitTest(const ImVec2& point) const {
    if (PointInRectangle(point, 650, 10, 140, 60)) return UiControl::AdminBack;
    for (int digit = 1; digit <= 9; ++digit) {
        const int row = (digit - 1) / 3;
        const int column = (digit - 1) % 3;
        if (PointInRectangle(point, kStartX + column * kSpacingX,
                             kStartY + row * kSpacingY, kButtonWidth, kButtonHeight)) {
            return static_cast<UiControl>(static_cast<int>(UiControl::Key1) + digit - 1);
        }
    }
    return UiControl::None;
}

void AdminPinPage::Reset() {
    value_.clear();
    last_digit_ = -1;
    last_digit_time_ = 0.0;
}

void AdminPinPage::AppendDigit(int digit, double ui_time) {
    if (digit < 1 || digit > 9 || value_.size() >= 4) return;
    value_.push_back(static_cast<char>('0' + digit));
    last_digit_ = digit;
    last_digit_time_ = ui_time;
}

void AdminPinPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {40, 40}, {760, 450}, 14.0f, true);
    drawing.CenteredText(&draw, drawing.SemiBold(), 28.0f, 67.0f, theme::Text,
                         "Voer beheerders-pincode in");
    drawing.CenteredText(&draw, drawing.Regular(), 18.0f, 108.0f, theme::MutedText,
                         "Pincode");
    const float dot_start = 400.0f - 3.0f * 17.0f;
    for (int index = 0; index < 4; ++index) {
        const ImVec2 center{dot_start + index * 34.0f, 151.0f};
        if (index < static_cast<int>(value_.size())) {
            draw.AddCircleFilled(center, 6.0f, theme::Blue, 20);
        } else {
            draw.AddCircle(center, 6.0f, theme::LightBlue, 20, 2.0f);
        }
    }
    const float now = static_cast<float>(ImGui::GetTime());
    for (int digit = 1; digit <= 9; ++digit) {
        const int row = (digit - 1) / 3;
        const int column = (digit - 1) % 3;
        const ImVec2 center{kStartX + column * kSpacingX + kButtonWidth * 0.5f,
                            kStartY + row * kSpacingY + kButtonHeight * 0.5f};
        const float elapsed = now - static_cast<float>(last_digit_time_);
        const bool highlighted = digit == last_digit_ && elapsed >= 0.0f && elapsed < 0.18f;
        const float remaining = highlighted ? 1.0f - elapsed / 0.18f : 0.0f;
        const float scale = highlighted ? 1.0f + 0.12f * remaining : 1.0f;
        draw.AddCircleFilled(center, 38.0f * scale,
                             highlighted ? theme::PressedSurface : theme::White, 32);
        draw.AddCircle(center, 38.0f * scale,
                       highlighted ? theme::Blue : theme::LightBlue, 32,
                       highlighted ? 2.5f : 1.5f);
        char text[2] = {static_cast<char>('0' + digit), '\0'};
        const ImVec2 size = drawing.SemiBold()->CalcTextSizeA(22.0f, FLT_MAX, 0.0f, text);
        draw.AddText(drawing.SemiBold(), 22.0f,
                     {center.x - size.x * 0.5f, center.y - size.y * 0.5f},
                     highlighted ? theme::Blue : theme::Text, text);
    }
    drawing.ActionButton(&draw, {650, 10}, {790, 70},
                         drawing.IconsAvailable() ? u8"\uE004Terug" : "Terug",
                         UiControl::AdminBack, ButtonVariant::Secondary);
}

}  // namespace clockin
