#include "pages/SignaturePage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/ActionButtonLayout.hpp"
#include "ui/components/PageDrawing.hpp"

#include <cstdio>
#include <utility>

namespace clockin {

UiControl SignaturePage::HitTest(const ImVec2& point) const {
    if (CenteredRightActionButton(0, 3).Contains(point)) {
        return UiControl::SignatureClear;
    }
    if (CenteredRightActionButton(1, 3).Contains(point)) {
        return UiControl::SignatureCancel;
    }
    if (CenteredRightActionButton(2, 3).Contains(point)) {
        return UiControl::SignatureSubmit;
    }
    return UiControl::None;
}

void SignaturePage::Start(std::string user_name) {
    user_name_ = std::move(user_name);
    warning_.clear();
    signature_.Clear();
}

bool SignaturePage::IsSignatureArea(const ImVec2& point) const {
    return PointInRectangle(point, 50, 150, 550, 270);
}

void SignaturePage::BeginStroke(const ImVec2& point) { signature_.Begin(point); }
void SignaturePage::ContinueStroke(const ImVec2& point) { signature_.Add(point); }

void SignaturePage::EndStroke() {
    signature_.End();
    if (signature_.TooComplex()) {
        warning_ = "Handtekening is te complex; wis en probeer opnieuw";
    }
}

void SignaturePage::CancelStroke() { signature_.CancelStroke(); }

void SignaturePage::Clear() {
    signature_.Clear();
    warning_.clear();
}

bool SignaturePage::BuildPayload(std::string& svg) {
    warning_.clear();
    if (signature_.TooComplex()) {
        warning_ = "Handtekening is te complex; wis en probeer opnieuw";
        return false;
    }
    if (signature_.SerializeSvg(svg)) return true;
    warning_ = signature_.IsValid() ? "Handtekening is te groot; wis en probeer opnieuw"
                                    : "Teken eerst uw handtekening";
    return false;
}

void SignaturePage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    char header[192];
    std::snprintf(header, sizeof(header), "Welkom, %s!", user_name_.c_str());
    drawing.CenteredText(&draw, drawing.SemiBold(), 28.0f, 28.0f, theme::Text,
                         header, 320.0f);
    drawing.CenteredText(&draw, drawing.Regular(), 19.0f, 78.0f, theme::MutedText,
                         "Zet hieronder uw handtekening om in te klokken", 320.0f);
    if (!warning_.empty()) {
        drawing.CenteredWrappedText(&draw, drawing.Regular(), 17.0f, 45.0f, 595.0f,
                                    108.0f, theme::RedText, warning_.c_str());
    }
    drawing.Surface(&draw, {50, 150}, {600, 420}, 10.0f, false);
    draw.AddLine({85, 380}, {565, 380}, theme::Border, 1.0f);
    for (const auto& stroke : signature_.Strokes()) {
        if (stroke.size() > 1) {
            draw.AddPolyline(stroke.data(), static_cast<int>(stroke.size()),
                             theme::Text, false, 3.0f);
        }
    }
    if (signature_.CurrentStroke().size() > 1) {
        draw.AddPolyline(signature_.CurrentStroke().data(),
                         static_cast<int>(signature_.CurrentStroke().size()),
                         theme::Text, false, 3.0f);
    }
    const ActionButtonBounds clear_button = CenteredRightActionButton(0, 3);
    const ActionButtonBounds cancel_button = CenteredRightActionButton(1, 3);
    const ActionButtonBounds submit_button = CenteredRightActionButton(2, 3);
    drawing.ActionButton(&draw, clear_button.minimum, clear_button.maximum,
                         drawing.IconsAvailable() ? u8"\uE001Wissen" : "Wissen",
                         UiControl::SignatureClear, ButtonVariant::Quiet);
    drawing.ActionButton(&draw, cancel_button.minimum, cancel_button.maximum,
                         drawing.IconsAvailable() ? u8"\uE002Annuleren" : "Annuleren",
                         UiControl::SignatureCancel, ButtonVariant::Secondary);
    drawing.ActionButton(&draw, submit_button.minimum, submit_button.maximum,
                         drawing.IconsAvailable() ? u8"\uE003Versturen" : "Versturen",
                         UiControl::SignatureSubmit, ButtonVariant::Primary);
}

}  // namespace clockin
