#include "pages/ProcessingPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <cmath>

namespace clockin {

void ProcessingPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {165, 95}, {635, 390}, 16.0f, true);
    const ImVec2 center{400.0f, 215.0f};
    const float start_angle =
        static_cast<float>(std::fmod(drawing.NowSeconds() * 5.0, 6.283185307179586));
    constexpr int segments = 24;
    for (int segment = 0; segment < segments; ++segment) {
        const float phase = static_cast<float>(segment) / segments;
        const float angle_start = start_angle + phase * 6.2831853f;
        const float angle_end = start_angle + (phase + 0.7f / segments) * 6.2831853f;
        const int alpha = 45 + static_cast<int>(phase * 210.0f);
        draw.AddLine({center.x + std::cos(angle_start) * 48.0f,
                      center.y + std::sin(angle_start) * 48.0f},
                     {center.x + std::cos(angle_end) * 48.0f,
                      center.y + std::sin(angle_end) * 48.0f},
                     PageDrawing::WithAlpha(theme::Blue, alpha), 4.0f);
    }
    drawing.CenteredWrappedText(&draw, drawing.SemiBold(), 27.0f, 205.0f, 595.0f,
                                305.0f, theme::Text,
                                message_.empty() ? "Even geduld..." : message_.c_str());
}

}  // namespace clockin
