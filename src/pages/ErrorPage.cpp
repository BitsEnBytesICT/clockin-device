#include "pages/ErrorPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/StatusAnimation.hpp"

namespace clockin {

void ErrorPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {110, 65}, {690, 420}, 16.0f, true);
    const ImVec2 center{400, 180};
    DrawErrorAnimation(draw, center,
                       drawing.NowSeconds() - animation_started_at_);
    drawing.CenteredWrappedText(&draw, drawing.SemiBold(), 27.0f, 145.0f, 655.0f,
                                275.0f, theme::RedText, message_.c_str());
    if (!detail_.empty()) {
        drawing.CenteredWrappedText(&draw, drawing.Regular(), 19.0f, 145.0f, 655.0f,
                                    335.0f, theme::MutedText, detail_.c_str());
    }
}

}  // namespace clockin
