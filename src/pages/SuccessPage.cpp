#include "pages/SuccessPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/StatusAnimation.hpp"

namespace clockin {

void SuccessPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {145, 75}, {655, 410}, 16.0f, true);
    const ImVec2 center{400, 190};
    DrawSuccessAnimation(draw, center,
                         drawing.NowSeconds() - animation_started_at_);
    const char* message = action_ == "clock_in" ? "Succesvol ingeklokt!"
                                                 : "Succesvol uitgeklokt!";
    drawing.CenteredText(&draw, drawing.SemiBold(), 31.0f, 285.0f, theme::Text, message);
    drawing.CenteredWrappedText(&draw, drawing.Regular(), 23.0f, 180.0f, 620.0f,
                                340.0f, theme::MutedText, user_name_.c_str());
}

}  // namespace clockin
