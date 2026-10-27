#include "pages/WaitingPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"

#include <algorithm>
#include <cmath>

namespace clockin {

UiControl WaitingPage::HitTest(const ImVec2& point) const {
    return PointInRectangle(point, 650, 10, 140, 60) ? UiControl::Admin : UiControl::None;
}

void WaitingPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {60, 120}, {740, 410}, 16.0f, true);
    const ImVec2 center{400.0f, 220.0f};
    // Phase is derived in double precision: the steady clock counts from boot,
    // and a float loses sub-frame resolution after a few days of uptime.
    const double time = drawing.NowSeconds();
    for (int ring = 0; ring < 2; ++ring) {
        float phase = static_cast<float>(std::fmod(time - ring * 0.5, 1.0));
        if (phase < 0.0f) phase += 1.0f;
        // Ease the ripple outwards and fade it in, so a ring never pops into view.
        const float remaining = 1.0f - phase;
        const float radius = 7.0f + 53.0f * (1.0f - remaining * remaining);
        const float fade_in = std::min(1.0f, phase / 0.12f);
        draw.AddCircle(center, radius,
                       PageDrawing::WithAlpha(theme::Blue,
                                              static_cast<int>(fade_in * remaining * 220.0f)),
                       0, 4.0f);
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
