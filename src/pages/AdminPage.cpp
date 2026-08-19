#include "pages/AdminPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

namespace clockin {

UiControl AdminPage::HitTest(const ImVec2& point) const {
    return PointInRectangle(point, 650, 10, 140, 60) ? UiControl::AdminBack
                                                      : UiControl::None;
}

void AdminPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.CenteredText(&draw, drawing.SemiBold(), 30.0f, 34.0f, theme::Text,
                         "Beheermenu");
    drawing.Surface(&draw, {90, 105}, {610, 310}, 14.0f, true);
    drawing.Text(&draw, drawing.SemiBold(), 18.0f, {125, 145}, theme::Blue,
                 "Laatst gelezen kaartnummer");
    drawing.Text(&draw, drawing.Regular(), 26.0f, {125, 190}, theme::Text,
                 uid_.empty() ? "Nog geen kaart gelezen" : uid_.c_str());
    drawing.Text(&draw, drawing.Regular(), 17.0f, {125, 250}, theme::MutedText,
                 "Scan een kaart om het kaartnummer te controleren.");
    drawing.ActionButton(&draw, {650, 10}, {790, 70},
                         drawing.IconsAvailable() ? u8"\uE004Terug" : "Terug",
                         UiControl::AdminBack, ButtonVariant::Secondary);
}

}  // namespace clockin
