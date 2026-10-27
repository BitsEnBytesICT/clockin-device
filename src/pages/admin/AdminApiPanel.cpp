#include "pages/admin/AdminApiPanel.hpp"

#include "domain/ApiUrl.hpp"
#include "pages/Page.hpp"
#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/PageTransition.hpp"
#include "ui/components/VectorIcons.hpp"

#include <cfloat>
#include <utility>

namespace clockin {
namespace {

constexpr float kUrlFontSize = 18.0f;
constexpr float kUrlMaximumWidth = 590.0f;

ImU32 StateColor(bool known, bool online) {
    if (!known) return theme::MutedText;
    return online ? theme::GreenText : theme::RedText;
}

std::string FitUrl(ImFont* font, const std::string& url) {
    if (font->CalcTextSizeA(kUrlFontSize, FLT_MAX, 0.0f, url.c_str()).x <= kUrlMaximumWidth) {
        return url;
    }
    std::string fitted = url;
    while (!fitted.empty() &&
           font->CalcTextSizeA(kUrlFontSize, FLT_MAX, 0.0f, (fitted + "...").c_str()).x >
               kUrlMaximumWidth) {
        fitted.pop_back();
    }
    return fitted + "...";
}

}  // namespace

void AdminApiPanel::Reset() {
    mode_ = Mode::Overview;
    message_.clear();
    message_is_error_ = false;
    keyboard_.Clear();
    mode_changed_at_ = 0.0;
}

void AdminApiPanel::SetApiStatus(bool known, bool online) {
    api_known_ = known;
    api_online_ = online;
}

UiControl AdminApiPanel::HitTest(const ImVec2& point) const {
    if (mode_ == Mode::Edit) return keyboard_.HitTest(point);
    return PointInRectangle(point, 260, 397, 280, 62) ? UiControl::AdminApiEdit
                                                      : UiControl::None;
}

AdminAction AdminApiPanel::Activate(const AppEvent& event) {
    AdminAction action;
    if (mode_ == Mode::Overview) {
        if (event.control == UiControl::AdminApiEdit) BeginEdit(event.occurred_at);
        return action;
    }
    if (event.control == UiControl::AdminKeyboardCancel) {
        Cancel(event.occurred_at);
        return action;
    }
    if (event.control == UiControl::AdminKeyboardConnect) {
        std::string normalized;
        std::string error;
        if (!NormalizeApiBaseUrl(keyboard_.Value(), normalized, error)) {
            keyboard_.SetError(error);
            return action;
        }
        if (normalized == current_url_) {
            message_ = "Het serveradres is niet gewijzigd.";
            message_is_error_ = false;
            keyboard_.Clear();
            mode_ = Mode::Overview;
            mode_changed_at_ = event.occurred_at;
            return action;
        }
        action.type = AdminActionType::SaveApiUrl;
        action.api_url = normalized;
        return action;
    }
    keyboard_.Activate(event);
    return action;
}

void AdminApiPanel::CompleteSave(bool succeeded,
                                 const std::string& error,
                                 double now_seconds) {
    if (!succeeded) {
        keyboard_.SetError(error.empty() ? "Opslaan is mislukt. Probeer opnieuw." : error);
        return;
    }
    keyboard_.Clear();
    message_ = "Opgeslagen. De verbinding wordt gecontroleerd.";
    message_is_error_ = false;
    mode_ = Mode::Overview;
    mode_changed_at_ = now_seconds;
}

void AdminApiPanel::Cancel(double now_seconds) {
    if (mode_ != Mode::Edit) return;
    keyboard_.Clear();
    mode_ = Mode::Overview;
    mode_changed_at_ = now_seconds;
}

void AdminApiPanel::BeginEdit(double now_seconds) {
    KeyboardOptions options;
    options.title = "Adres van de managementserver";
    options.placeholder = "https://server.voorbeeld.nl";
    options.confirm_label = "Opslaan";
    options.confirm_icon = VectorIcon::Check;
    options.secret = false;
    options.max_length = kMaxApiBaseUrlLength;
    keyboard_.Open(std::move(options), current_url_);
    message_.clear();
    mode_ = Mode::Edit;
    mode_changed_at_ = now_seconds;
}

void AdminApiPanel::Render(ImDrawList& draw, PageDrawing& drawing) const {
    const int first_vertex = draw.VtxBuffer.Size;
    if (mode_ == Mode::Edit) {
        keyboard_.Render(draw, drawing);
    } else {
        drawing.Surface(&draw, {48, 112}, {752, 487}, 14.0f, true);
        draw.AddCircleFilled({400, 190}, 43.0f, theme::PressedSurface, 0);
        DrawVectorIcon(draw, VectorIcon::Globe, {400, 190}, 39.0f, theme::Blue);
        drawing.CenteredText(&draw, drawing.SemiBold(), 22.0f, 241.0f, theme::Text,
                             "Managementserver");
        draw.AddRectFilled({90, 276}, {710, 322}, theme::OffWhite, 10.0f);
        draw.AddRect({90, 276}, {710, 322}, theme::Border, 10.0f);
        const std::string url = current_url_.empty() ? std::string("Niet ingesteld")
                                                      : FitUrl(drawing.SemiBold(), current_url_);
        drawing.CenteredText(&draw, drawing.SemiBold(), kUrlFontSize, 288.0f,
                             current_url_.empty() ? theme::MutedText : theme::Text,
                             url.c_str());

        const char* status = !api_known_ ? "Verbinding controleren..."
                             : api_online_ ? "Server bereikbaar"
                                           : "Server niet bereikbaar";
        const ImU32 status_color = StateColor(api_known_, api_online_);
        const ImVec2 status_size =
            drawing.Regular()->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, status);
        const float status_x = 400.0f - (status_size.x + 14.0f) * 0.5f;
        draw.AddCircleFilled({status_x + 5.0f, 345.0f}, 5.0f, status_color, 0);
        drawing.Text(&draw, drawing.Regular(), 15.0f, {status_x + 14.0f, 336.0f},
                     status_color, status);
        if (!message_.empty()) {
            drawing.CenteredText(&draw, drawing.Regular(), 14.0f, 366.0f,
                                 message_is_error_ ? theme::RedText : theme::GreenText,
                                 message_.c_str());
        }
        drawing.ActionButton(&draw, {260, 397}, {540, 459}, "Adres wijzigen",
                             UiControl::AdminApiEdit, ButtonVariant::Primary,
                             VectorIcon::Globe);
    }
    ApplyPageTransition(
        draw, first_vertex,
        EvaluatePageTransition(drawing.NowSeconds() - mode_changed_at_));
}

}  // namespace clockin
