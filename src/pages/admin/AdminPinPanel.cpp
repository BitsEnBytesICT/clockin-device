#include "pages/admin/AdminPinPanel.hpp"

#include "pages/Page.hpp"
#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/PageTransition.hpp"
#include "ui/components/PinKeypad.hpp"
#include "ui/components/VectorIcons.hpp"

#include <algorithm>
#include <cmath>

namespace clockin {
namespace {

constexpr PinKeypadBounds kKeypad{{270.0f, 210.0f}, {530.0f, 448.0f}};

}  // namespace

void AdminPinPanel::Reset(bool persistable) {
    ClearSecrets();
    persistable_ = persistable;
    mode_ = Mode::Overview;
    message_.clear();
    mode_changed_at_ = 0.0;
}

UiControl AdminPinPanel::HitTest(const ImVec2& point) const {
    if (mode_ == Mode::Overview || mode_ == Mode::Saved) {
        return persistable_ && PointInRectangle(point, 260, 300, 280, 62)
                   ? UiControl::AdminPinChange
                   : UiControl::None;
    }
    if (mode_ == Mode::EnterNew || mode_ == Mode::Confirm) {
        if (mode_ == Mode::Confirm && !message_.empty() &&
            PointInRectangle(point, 68, 325, 150, 50)) {
            return UiControl::AdminPinRestart;
        }
        if (PointInRectangle(point, 68, 387, 150, 54)) return UiControl::AdminPinCancel;
        return PinKeypad::HitTest(point, kKeypad);
    }
    return UiControl::None;
}

AdminAction AdminPinPanel::Activate(const AppEvent& event) {
    if (event.control == UiControl::AdminPinChange) {
        BeginEntry(event.occurred_at);
        return {};
    }
    if (event.control == UiControl::AdminPinCancel) {
        CancelSubflow(event.occurred_at);
        return {};
    }
    if (event.control == UiControl::AdminPinRestart) {
        BeginEntry(event.occurred_at);
        return {};
    }
    if (mode_ != Mode::EnterNew && mode_ != Mode::Confirm) return {};
    std::string& value = ActiveValue();
    if (!PinKeypad::Apply(event.control, value)) return {};
    last_key_ = event.control;
    last_key_time_ = event.occurred_at;
    message_.clear();
    if (value.size() != 4) return {};
    if (mode_ == Mode::EnterNew) {
        mode_ = Mode::Confirm;
        mode_changed_at_ = event.occurred_at;
        last_key_ = UiControl::None;
        return {};
    }
    if (confirm_pin_ != first_pin_) {
        std::fill(confirm_pin_.begin(), confirm_pin_.end(), '\0');
        confirm_pin_.clear();
        message_ = "De pincodes zijn niet gelijk. Probeer de bevestiging opnieuw.";
        return {};
    }
    mode_ = Mode::Saving;
    mode_changed_at_ = event.occurred_at;
    AdminAction action;
    action.type = AdminActionType::SavePin;
    action.pin = first_pin_;
    return action;
}

void AdminPinPanel::CompleteSave(bool succeeded,
                                 const std::string& message,
                                 double now_seconds) {
    if (succeeded) {
        ClearSecrets();
        mode_ = Mode::Saved;
        message_ = "De nieuwe pincode is veilig opgeslagen.";
    } else {
        std::fill(confirm_pin_.begin(), confirm_pin_.end(), '\0');
        confirm_pin_.clear();
        mode_ = Mode::Confirm;
        message_ = message.empty() ? "Opslaan is mislukt. Probeer opnieuw." : message;
    }
    mode_changed_at_ = now_seconds;
}

bool AdminPinPanel::HasSubflow() const {
    return mode_ == Mode::EnterNew || mode_ == Mode::Confirm || mode_ == Mode::Saving;
}

void AdminPinPanel::CancelSubflow(double now_seconds) {
    ClearSecrets();
    mode_ = Mode::Overview;
    message_.clear();
    mode_changed_at_ = now_seconds;
}

void AdminPinPanel::ClearSecrets() {
    std::fill(first_pin_.begin(), first_pin_.end(), '\0');
    std::fill(confirm_pin_.begin(), confirm_pin_.end(), '\0');
    first_pin_.clear();
    confirm_pin_.clear();
    last_key_ = UiControl::None;
}

std::string& AdminPinPanel::ActiveValue() {
    return mode_ == Mode::EnterNew ? first_pin_ : confirm_pin_;
}

const std::string& AdminPinPanel::ActiveValue() const {
    return mode_ == Mode::EnterNew ? first_pin_ : confirm_pin_;
}

void AdminPinPanel::BeginEntry(double now_seconds) {
    ClearSecrets();
    message_.clear();
    mode_ = Mode::EnterNew;
    mode_changed_at_ = now_seconds;
}

void AdminPinPanel::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Surface(&draw, {48, 147}, {752, 452}, 14.0f, true);
    const int first_vertex = draw.VtxBuffer.Size;
    if (mode_ == Mode::Overview || mode_ == Mode::Saved) {
        draw.AddCircleFilled({400, 226}, 46.0f, theme::PressedSurface, 36);
        DrawVectorIcon(draw, mode_ == Mode::Saved ? VectorIcon::Check : VectorIcon::Pin,
                       {400, 226}, 39.0f,
                       mode_ == Mode::Saved ? theme::GreenText : theme::Blue);
        drawing.CenteredText(&draw, drawing.SemiBold(), 22.0f, 276.0f, theme::Text,
                             mode_ == Mode::Saved ? "Pincode aangepast" : "Beheerderspincode");
        drawing.ActionButton(&draw, {260, 300}, {540, 362},
                             mode_ == Mode::Saved ? "Nogmaals aanpassen"
                                                  : "Pincode aanpassen",
                             UiControl::AdminPinChange, ButtonVariant::Primary,
                             VectorIcon::Pin);
        if (!persistable_) {
            drawing.CenteredWrappedText(
                &draw, drawing.Regular(), 15.0f, 180.0f, 620.0f, 378.0f,
                theme::RedText,
                "De pincode komt uit een vaste omgevingsvariabele en kan hier niet worden opgeslagen.");
        } else if (!message_.empty()) {
            drawing.CenteredText(&draw, drawing.Regular(), 15.0f, 384.0f,
                                 theme::GreenText, message_.c_str());
        }
    } else if (mode_ == Mode::Saving) {
        DrawVectorIcon(draw, VectorIcon::Refresh, {400, 275}, 48.0f, theme::Blue,
                       static_cast<float>(std::fmod(drawing.NowSeconds() * 1.8, 1.0)));
        drawing.CenteredText(&draw, drawing.SemiBold(), 21.0f, 320.0f, theme::Text,
                             "Pincode veilig opslaan...");
    } else {
        drawing.CenteredText(&draw, drawing.SemiBold(), 19.0f, 160.0f, theme::Text,
                             mode_ == Mode::EnterNew ? "Nieuwe pincode"
                                                     : "Herhaal de nieuwe pincode");
        const std::string& value = ActiveValue();
        const float dot_start = 400.0f - 3.0f * 14.0f;
        for (int index = 0; index < 4; ++index) {
            const ImVec2 center{dot_start + index * 28.0f, 194.0f};
            if (index < static_cast<int>(value.size())) {
                draw.AddCircleFilled(center, 5.0f, theme::Blue, 18);
            } else {
                draw.AddCircle(center, 5.0f, theme::LightBlue, 18, 1.8f);
            }
        }
        PinKeypad::Render(draw, drawing, kKeypad, last_key_, last_key_time_);
        drawing.ActionButton(&draw, {68, 387}, {218, 441}, "Annuleren",
                             UiControl::AdminPinCancel, ButtonVariant::Secondary,
                             VectorIcon::Close);
        if (!message_.empty()) {
            drawing.CenteredWrappedText(&draw, drawing.Regular(), 14.0f, 52.0f, 250.0f,
                                        267.0f, theme::RedText, message_.c_str());
            drawing.ActionButton(&draw, {68, 325}, {218, 375}, "Opnieuw",
                                 UiControl::AdminPinRestart, ButtonVariant::Secondary,
                                 VectorIcon::Refresh);
        }
    }
    ApplyPageTransition(
        draw, first_vertex,
        EvaluatePageTransition(drawing.NowSeconds() - mode_changed_at_));
}

}  // namespace clockin
