#include "ui/components/PinKeypad.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <cfloat>

namespace clockin {
namespace {

constexpr float kGap = 8.0f;

UiControl ControlForCell(int row, int column) {
    if (row < 3) {
        return static_cast<UiControl>(static_cast<int>(UiControl::Key1) + row * 3 + column);
    }
    if (column == 1) return UiControl::Key0;
    if (column == 2) return UiControl::KeyBackspace;
    return UiControl::None;
}

void CellBounds(const PinKeypadBounds& bounds,
                int row,
                int column,
                ImVec2& minimum,
                ImVec2& maximum) {
    const float width = (bounds.maximum.x - bounds.minimum.x - 2.0f * kGap) / 3.0f;
    const float height = (bounds.maximum.y - bounds.minimum.y - 3.0f * kGap) / 4.0f;
    minimum = {bounds.minimum.x + column * (width + kGap),
               bounds.minimum.y + row * (height + kGap)};
    maximum = {minimum.x + width, minimum.y + height};
}

}  // namespace

UiControl PinKeypad::HitTest(const ImVec2& point, const PinKeypadBounds& bounds) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 3; ++column) {
            const UiControl control = ControlForCell(row, column);
            if (control == UiControl::None) continue;
            ImVec2 minimum;
            ImVec2 maximum;
            CellBounds(bounds, row, column, minimum, maximum);
            if (point.x >= minimum.x && point.x <= maximum.x &&
                point.y >= minimum.y && point.y <= maximum.y) {
                return control;
            }
        }
    }
    return UiControl::None;
}

int PinKeypad::Digit(UiControl control) {
    if (control == UiControl::Key0) return 0;
    if (control >= UiControl::Key1 && control <= UiControl::Key9) {
        return static_cast<int>(control) - static_cast<int>(UiControl::Key1) + 1;
    }
    return -1;
}

bool PinKeypad::Apply(UiControl control, std::string& value, std::size_t maximum) {
    const int digit = Digit(control);
    if (digit >= 0 && value.size() < maximum) {
        value.push_back(static_cast<char>('0' + digit));
        return true;
    }
    if (control == UiControl::KeyBackspace && !value.empty()) {
        value.pop_back();
        return true;
    }
    return false;
}

void PinKeypad::Render(ImDrawList& draw,
                       PageDrawing& drawing,
                       const PinKeypadBounds& bounds,
                       UiControl last_key,
                       double last_key_time) {
    const double elapsed = drawing.NowSeconds() - last_key_time;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 3; ++column) {
            const UiControl control = ControlForCell(row, column);
            if (control == UiControl::None) continue;
            ImVec2 minimum;
            ImVec2 maximum;
            CellBounds(bounds, row, column, minimum, maximum);
            const bool highlighted = control == last_key && elapsed >= 0.0 && elapsed < 0.18;
            const float remaining = highlighted
                                        ? static_cast<float>(1.0 - elapsed / 0.18)
                                        : 0.0f;
            const ImVec2 center{(minimum.x + maximum.x) * 0.5f,
                                (minimum.y + maximum.y) * 0.5f};
            const float scale = 1.0f + 0.035f * remaining * remaining;
            const ImVec2 half{(maximum.x - minimum.x) * 0.5f * scale,
                              (maximum.y - minimum.y) * 0.5f * scale};
            const ImVec2 drawn_min{center.x - half.x, center.y - half.y};
            const ImVec2 drawn_max{center.x + half.x, center.y + half.y};
            draw.AddRectFilled(drawn_min, drawn_max,
                               highlighted ? theme::PressedSurface : theme::White, 11.0f);
            draw.AddRect(drawn_min, drawn_max,
                         highlighted ? theme::Blue : theme::Border, 11.0f, 0,
                         highlighted ? 2.0f : 1.0f);
            std::string label;
            const int digit = Digit(control);
            if (digit >= 0) label.assign(1, static_cast<char>('0' + digit));
            else label = "<";
            const ImVec2 size = drawing.SemiBold()->CalcTextSizeA(
                21.0f, FLT_MAX, 0.0f, label.c_str());
            draw.AddText(drawing.SemiBold(), 21.0f,
                         {center.x - size.x * 0.5f, center.y - size.y * 0.5f},
                         highlighted ? theme::Blue : theme::Text, label.c_str());
        }
    }
}

}  // namespace clockin
