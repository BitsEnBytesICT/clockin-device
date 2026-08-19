#include "pages/SuccessPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

namespace clockin {

void SuccessPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {145, 75}, {655, 410}, 16.0f, true);
    const ImVec2 center{400, 190};
    draw.AddCircleFilled(center, 60.0f, theme::Green, 40);
    draw.AddLine({center.x - 25, center.y}, {center.x - 5, center.y + 20},
                 theme::GreenText, 6.0f);
    draw.AddLine({center.x - 5, center.y + 20}, {center.x + 30, center.y - 25},
                 theme::GreenText, 6.0f);
    const char* message = action_ == "clock_in" ? "Succesvol ingeklokt!"
                                                 : "Succesvol uitgeklokt!";
    drawing.CenteredText(&draw, drawing.SemiBold(), 31.0f, 285.0f, theme::Text, message);
    drawing.CenteredWrappedText(&draw, drawing.Regular(), 23.0f, 180.0f, 620.0f,
                                340.0f, theme::MutedText, user_name_.c_str());
}

}  // namespace clockin
