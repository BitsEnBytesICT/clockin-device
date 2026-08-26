#include "pages/WaitingPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <cmath>

namespace clockin {

UiControl WaitingPage::HitTest(const ImVec2& point) const {
    return PointInRectangle(point, 650, 10, 140, 60) ? UiControl::Admin : UiControl::None;
}

void WaitingPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {60, 120}, {740, 410}, 16.0f, true);
    const ImVec2 center{400.0f, 220.0f};
    const float time = static_cast<float>(ImGui::GetTime());
    for (int ring = 0; ring < 2; ++ring) {
        float phase = std::fmod(time - ring * 0.5f, 1.0f);
        if (phase < 0.0f) phase += 1.0f;
        if (phase < 0.05f) continue;
        draw.AddCircle(center, 7.0f + 53.0f * phase,
                       PageDrawing::WithAlpha(theme::Blue,
                                              static_cast<int>((1.0f - phase) * 220.0f)),
                       32, 4.0f);
    }
    drawing.CenteredText(&draw, drawing.SemiBold(), 32.0f, 304.0f, theme::Text,
                         "Scan uw RFID-kaart");
    drawing.CenteredText(&draw, drawing.Regular(), 20.0f, 350.0f, theme::MutedText,
                         "Houd uw kaart voor de lezer");
    drawing.ActionButton(&draw, {650, 10}, {790, 70},
                         drawing.IconsAvailable() ? u8"\uE000Beheer" : "Beheer",
                         UiControl::Admin, ButtonVariant::Quiet);
    if (api_known_ && !api_online_) {
        draw.AddText({18, 448}, theme::RedText,
                     "API offline - nieuwe scans kunnen mislukken");
    }
}

}  // namespace clockin
