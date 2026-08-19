#include "pages/AttendancePage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace clockin {

UiControl AttendancePage::HitTest(const ImVec2& point) const {
    if (PointInRectangle(point, 650, 10, 140, 60)) return UiControl::AttendanceBack;
    if (PointInRectangle(point, 650, 400, 140, 60)) return UiControl::AttendanceConfirm;
    return UiControl::None;
}

void AttendancePage::SetData(std::string user_name,
                             std::vector<std::string> dates,
                             std::string warning) {
    user_name_ = std::move(user_name);
    dates_ = std::move(dates);
    warning_ = std::move(warning);
    scroll_offset_ = 0.0f;
    dragging_ = false;
}

void AttendancePage::Drag(float delta_y) {
    const float maximum_scroll =
        std::max(0.0f, 24.0f * static_cast<float>(dates_.size()) - 280.0f);
    scroll_offset_ = std::max(-maximum_scroll,
                              std::min(0.0f, scroll_offset_ + delta_y));
}

void AttendancePage::Reset() {
    user_name_.clear();
    dates_.clear();
    warning_.clear();
    scroll_offset_ = 0.0f;
    dragging_ = false;
}

void AttendancePage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.CenteredText(&draw, drawing.SemiBold(), 27.0f, 20.0f, theme::Text,
                         "Aanwezigheid", 320.0f);
    char user_label[192];
    std::snprintf(user_label, sizeof(user_label), "Gebruiker: %s",
                  user_name_.empty() ? "(onbekend)" : user_name_.c_str());
    drawing.Text(&draw, drawing.Regular(), 19.0f, {40, 60}, theme::MutedText, user_label);
    if (!warning_.empty()) {
        drawing.Text(&draw, drawing.Regular(), 17.0f, {40, 80}, theme::RedText,
                     warning_.c_str());
    }
    const ImVec2 table_minimum{40, 90};
    const ImVec2 table_maximum{600, 420};
    drawing.Surface(&draw, table_minimum, table_maximum, 12.0f, true);
    draw.AddRectFilled({45, 91}, {599, 136}, theme::LightWhite, 11.0f,
                       ImDrawFlags_RoundCornersTopRight);
    drawing.Text(&draw, drawing.SemiBold(), 18.0f, {62, 107}, theme::Blue,
                 "Laatste 30 dagen");
    draw.AddLine({50, 136}, {590, 136}, theme::Border, 1.0f);

    constexpr float start_y = 146.0f;
    constexpr float row_height = 24.0f;
    const float visible_height = table_maximum.y - start_y;
    const float total_height = row_height * static_cast<float>(dates_.size());
    const float maximum_scroll = std::max(0.0f, total_height - visible_height);
    const float scroll = std::max(-maximum_scroll, std::min(0.0f, scroll_offset_));
    if (dates_.empty()) {
        drawing.Text(&draw, drawing.Regular(), 18.0f, {62, 155}, theme::MutedText,
                     "Geen aanwezigheid in de laatste 30 dagen");
    } else {
        draw.PushClipRect({table_minimum.x + 5.0f, start_y}, table_maximum, true);
        for (std::size_t index = 0; index < dates_.size(); ++index) {
            const float y = start_y + static_cast<float>(index) * row_height + scroll;
            if (y >= start_y - row_height && y <= table_maximum.y) {
                drawing.Text(&draw, drawing.Regular(), 18.0f, {62, y}, theme::Text,
                             dates_[index].c_str());
            }
        }
        draw.PopClipRect();
        if (maximum_scroll > 0.0f) {
            constexpr float bar_minimum_x = 608.0f;
            constexpr float bar_maximum_x = 618.0f;
            draw.AddRectFilled({bar_minimum_x, start_y}, {bar_maximum_x, table_maximum.y},
                               theme::Border, 5.0f);
            const float thumb_height = std::max(
                24.0f, (visible_height / total_height) * (table_maximum.y - start_y));
            const float scroll_fraction =
                std::max(0.0f, std::min(1.0f, -scroll / maximum_scroll));
            const float thumb_y = start_y +
                                  (table_maximum.y - start_y - thumb_height) * scroll_fraction;
            draw.AddRectFilled({bar_minimum_x + 1.0f, thumb_y},
                               {bar_maximum_x - 1.0f, thumb_y + thumb_height},
                               theme::Blue, 4.0f);
        }
    }
    drawing.ActionButton(&draw, {650, 10}, {790, 70},
                         drawing.IconsAvailable() ? u8"\uE004Terug" : "Terug",
                         UiControl::AttendanceBack, ButtonVariant::Secondary);
    drawing.ActionButton(&draw, {650, 400}, {790, 460},
                         drawing.IconsAvailable() ? u8"\uE005Bevestig" : "Bevestig",
                         UiControl::AttendanceConfirm, ButtonVariant::Primary);
}

}  // namespace clockin
