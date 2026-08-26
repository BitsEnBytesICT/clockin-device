#include "pages/admin/AdminRfidPanel.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/VectorIcons.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace clockin {

void AdminRfidPanel::SetUid(std::string uid) {
    uid_ = std::move(uid);
    read_at_ = ImGui::GetTime();
}

void AdminRfidPanel::Reset() {
    uid_.clear();
    read_at_ = 0.0;
}

void AdminRfidPanel::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {48, 147}, {752, 452}, 14.0f, true);
    const ImVec2 icon_center{155.0f, 294.0f};
    const double elapsed = std::max(0.0, ImGui::GetTime() - read_at_);
    const float pulse = uid_.empty() ? 0.0f
                                     : static_cast<float>(std::exp(-elapsed * 2.6) *
                                                           (0.5 + 0.5 * std::sin(elapsed * 12.0)));
    if (pulse > 0.01f) {
        draw.AddCircle(icon_center, 54.0f + pulse * 13.0f,
                       PageDrawing::WithAlpha(theme::Blue, static_cast<int>(90 * pulse)),
                       40, 2.0f);
    }
    draw.AddCircleFilled(icon_center, 51.0f, theme::PressedSurface, 40);
    DrawVectorIcon(draw, VectorIcon::Card, icon_center, 43.0f, theme::Blue);
    drawing.Text(&draw, drawing.SemiBold(), 19.0f, {235, 198}, theme::Blue,
                 "RFID-kaart uitlezen");
    drawing.Text(&draw, drawing.Regular(), 17.0f, {235, 232}, theme::MutedText,
                 "Houd een kaart tegen de lezer.");
    drawing.Text(&draw, drawing.Regular(), 15.0f, {235, 285}, theme::MutedText,
                 "Laatst gelezen kaartnummer");
    drawing.Text(&draw, drawing.SemiBold(), 29.0f, {235, 316}, theme::Text,
                 uid_.empty() ? "Nog geen kaart gelezen" : uid_.c_str());
    const ImU32 status_color = uid_.empty() ? theme::MutedText : theme::GreenText;
    draw.AddCircleFilled({243, 384}, 5.0f, status_color, 16);
    drawing.Text(&draw, drawing.Regular(), 15.0f, {257, 374}, status_color,
                 uid_.empty() ? "Lezer gereed" : "Kaart succesvol gelezen");
}

}  // namespace clockin
