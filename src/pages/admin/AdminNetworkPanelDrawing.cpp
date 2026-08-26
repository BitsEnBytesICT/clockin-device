#include "pages/admin/AdminNetworkPanel.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/PageTransition.hpp"
#include "ui/components/VectorIcons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace clockin {
namespace {

constexpr float kListTop = 263.0f;
constexpr float kListBottom = 444.0f;
constexpr float kRowHeight = 58.0f;

ImU32 StateColor(bool known, bool online) {
    if (!known) return theme::MutedText;
    return online ? theme::GreenText : theme::RedText;
}

void DrawStatusValue(ImDrawList& draw,
                     PageDrawing& drawing,
                     const ImVec2& icon_center,
                     VectorIcon icon,
                     const char* label,
                     const std::string& value,
                     ImU32 color) {
    draw.AddCircleFilled(icon_center, 19.0f, theme::PressedSurface, 24);
    DrawVectorIcon(draw, icon, icon_center, 20.0f, color);
    drawing.Text(&draw, drawing.Regular(), 12.0f,
                 {icon_center.x + 29.0f, icon_center.y - 17.0f}, theme::MutedText,
                 label);
    drawing.Text(&draw, drawing.SemiBold(), 15.0f,
                 {icon_center.x + 29.0f, icon_center.y + 1.0f}, color,
                 value.c_str());
}

}  // namespace

void AdminNetworkPanel::Render(ImDrawList& draw, PageDrawing& drawing) const {
    if (mode_ == Mode::Password) {
        const int first_vertex = draw.VtxBuffer.Size;
        keyboard_.Render(draw, drawing, selected_ssid_);
        ApplyPageTransition(
            draw, first_vertex,
            EvaluatePageTransition(drawing.NowSeconds() - mode_changed_at_));
        return;
    }
    drawing.Surface(&draw, {48, 147}, {752, 452}, 14.0f, true);
    const int first_vertex = draw.VtxBuffer.Size;
    if (mode_ == Mode::List) RenderList(draw, drawing);
    else RenderDetails(draw, drawing);
    if (mode_ == Mode::ConfirmForget) RenderConfirmation(draw, drawing);
    ApplyPageTransition(
        draw, first_vertex,
        EvaluatePageTransition(drawing.NowSeconds() - mode_changed_at_));
}

void AdminNetworkPanel::RenderList(ImDrawList& draw, PageDrawing& drawing) const {
    const std::string wifi_value = snapshot_.associated
                                       ? snapshot_.current_ssid
                                       : NetworkStateLabel(snapshot_.state);
    DrawStatusValue(draw, drawing, {75, 180}, VectorIcon::Wifi, "Wi-Fi", wifi_value,
                    snapshot_.associated ? theme::GreenText : theme::MutedText);
    DrawStatusValue(draw, drawing, {308, 180}, VectorIcon::Globe, "IP-adres",
                    snapshot_.ipv4_address.empty() ? "Geen adres"
                                                   : snapshot_.ipv4_address,
                    snapshot_.ipv4_address.empty() ? theme::MutedText
                                                   : theme::Text);
    DrawStatusValue(draw, drawing, {514, 180}, VectorIcon::Check, "Managementserver",
                    !api_known_ ? "Controleren..."
                                : api_online_ ? "Bereikbaar" : "Niet bereikbaar",
                    StateColor(api_known_, api_online_));
    const bool scanning = snapshot_.state == NetworkState::Scanning;
    const ImVec2 refresh_center{711.5f, 180.5f};
    if (scanning) {
        const float pulse = 0.5f + 0.5f * std::sin(
            static_cast<float>(drawing.NowSeconds()) * 5.0f);
        draw.AddCircle(refresh_center, 31.0f + pulse * 3.0f,
                       PageDrawing::WithAlpha(theme::Blue, 55), 32, 2.0f);
    }
    drawing.IconButton(
        &draw, {684, 153}, {739, 208}, VectorIcon::Refresh,
        UiControl::AdminNetworkRefresh,
        scanning ? ButtonVariant::Primary : ButtonVariant::Secondary,
        scanning ? static_cast<float>(std::fmod(drawing.NowSeconds() * 1.25, 1.0))
                 : 0.0f);

    const std::string error = !local_error_.empty() ? local_error_ : snapshot_.last_error;
    if (!error.empty()) {
        drawing.Text(&draw, drawing.Regular(), 13.0f, {60, 224}, theme::RedText,
                     error.c_str());
    } else {
        drawing.Text(&draw, drawing.SemiBold(), 15.0f, {60, 225},
                     scanning ? theme::Blue : theme::Text,
                     scanning ? "Netwerken vernieuwen..."
                              : "Beschikbare netwerken");
    }
    if (snapshot_.networks.empty()) {
        if (scanning) {
            DrawVectorIcon(draw, VectorIcon::Refresh, {400, 335}, 37.0f, theme::Blue,
                           static_cast<float>(std::fmod(drawing.NowSeconds() * 1.8, 1.0)));
        }
        drawing.CenteredText(&draw, drawing.Regular(), 16.0f, 367.0f,
                             theme::MutedText,
                             scanning ? "Netwerken zoeken..."
                                      : "Tik op het vernieuw-icoon om netwerken te zoeken");
        return;
    }

    draw.PushClipRect({52, kListTop}, {748, kListBottom}, true);
    for (std::size_t index = 0; index < snapshot_.networks.size(); ++index) {
        const WifiNetwork& network = snapshot_.networks[index];
        const float y = kListTop + static_cast<float>(index) * kRowHeight + scroll_offset_;
        if (y + kRowHeight < kListTop || y > kListBottom) continue;
        const ImU32 fill = network.connected ? theme::PressedSurface
                                             : index % 2 == 0 ? theme::White
                                                              : theme::LightWhite;
        draw.AddRectFilled({57, y + 2}, {743, y + kRowHeight - 3}, fill, 9.0f);
        DrawWifiSignal(draw, {82, y + 29}, 28.0f, network.signal_dbm,
                       network.connected ? theme::GreenText : theme::Blue);
        drawing.Text(&draw, drawing.SemiBold(), 16.0f, {112, y + 9}, theme::Text,
                     network.ssid.c_str());
        std::string subtitle = WifiSecurityLabel(network.security, network.supports_sae);
        if (network.connected) subtitle += "  |  Verbonden";
        else if (network.known) subtitle += "  |  Opgeslagen";
        drawing.Text(&draw, drawing.Regular(), 13.0f, {112, y + 31},
                     network.security == WifiSecurity::Unsupported
                         ? theme::RedText
                         : network.connected ? theme::GreenText : theme::MutedText,
                     subtitle.c_str());
        if (network.security != WifiSecurity::Open) {
            DrawVectorIcon(draw, VectorIcon::Lock, {699, y + 28}, 19.0f,
                           network.security == WifiSecurity::Unsupported
                               ? theme::RedText
                               : theme::MutedText);
        }
    }
    draw.PopClipRect();
    const float content_height = static_cast<float>(snapshot_.networks.size()) * kRowHeight;
    const float view_height = kListBottom - kListTop;
    if (content_height > view_height) {
        const float maximum = content_height - view_height;
        const float thumb_height = std::max(28.0f, view_height * view_height / content_height);
        const float fraction = maximum <= 0.0f ? 0.0f : -scroll_offset_ / maximum;
        const float y = kListTop + (view_height - thumb_height) * fraction;
        draw.AddRectFilled({744, y}, {748, y + thumb_height}, theme::Blue, 2.0f);
    }
}

void AdminNetworkPanel::RenderDetails(ImDrawList& draw, PageDrawing& drawing) const {
    const WifiNetwork* network = Selected();
    drawing.ActionButton(&draw, {65, 163}, {210, 211}, "Netwerklijst",
                         UiControl::AdminNetworkDetailsBack, ButtonVariant::Secondary,
                         VectorIcon::Back);
    if (network == nullptr) return;
    draw.AddCircleFilled({400, 234}, 43.0f, theme::PressedSurface, 36);
    DrawVectorIcon(draw, VectorIcon::Wifi, {400, 234}, 39.0f,
                   network->connected ? theme::GreenText : theme::Blue);
    drawing.CenteredText(&draw, drawing.SemiBold(), 24.0f, 280.0f, theme::Text,
                         network->ssid.c_str());
    std::string detail = WifiSecurityLabel(network->security, network->supports_sae);
    if (network->connected) detail += "  |  Verbonden";
    else if (network->known) detail += "  |  Wachtwoord opgeslagen";
    drawing.CenteredText(&draw, drawing.Regular(), 15.0f, 313.0f,
                         network->security == WifiSecurity::Unsupported
                             ? theme::RedText
                             : theme::MutedText,
                         detail.c_str());
    const std::string error = !local_error_.empty() ? local_error_ : snapshot_.last_error;
    if (!error.empty()) {
        drawing.CenteredWrappedText(&draw, drawing.Regular(), 13.0f, 170.0f, 630.0f,
                                    339.0f, theme::RedText, error.c_str());
    }
    if (Busy()) {
        DrawVectorIcon(draw, VectorIcon::Refresh, {400, 398}, 32.0f, theme::Blue,
                       static_cast<float>(std::fmod(drawing.NowSeconds() * 1.8, 1.0)));
        drawing.CenteredText(&draw, drawing.Regular(), 14.0f, 422.0f, theme::MutedText,
                             NetworkStateLabel(snapshot_.state));
        return;
    }
    if (network->known) {
        drawing.ActionButton(&draw, {80, 371}, {330, 429}, "Netwerk vergeten",
                             UiControl::AdminNetworkForget, ButtonVariant::Secondary,
                             VectorIcon::Trash);
    }
    if (network->security == WifiSecurity::Unsupported) {
        drawing.CenteredText(&draw, drawing.Regular(), 15.0f, 390.0f, theme::RedText,
                             "Zakelijke EAP- en WEP-netwerken worden niet ondersteund.");
    } else {
        drawing.ActionButton(&draw, {470, 371}, {720, 429},
                             network->connected ? "Verbinding verbreken" : "Verbinden",
                             network->connected ? UiControl::AdminNetworkDisconnect
                                                : UiControl::AdminNetworkConnect,
                             network->connected ? ButtonVariant::Secondary
                                                : ButtonVariant::Primary,
                             network->connected ? VectorIcon::Disconnect
                                                : VectorIcon::Wifi);
    }
}

void AdminNetworkPanel::RenderConfirmation(ImDrawList& draw,
                                           PageDrawing& drawing) const {
    draw.AddRectFilled({0, 0}, {800, 480}, IM_COL32(22, 35, 45, 145));
    draw.AddRectFilled({170, 210}, {630, 420}, theme::White, 16.0f);
    draw.AddRect({170, 210}, {630, 420}, theme::Border, 16.0f);
    drawing.CenteredText(&draw, drawing.SemiBold(), 21.0f, 236.0f, theme::Text,
                         "Opgeslagen netwerk vergeten?");
    const WifiNetwork* network = Selected();
    const std::string name = network == nullptr ? std::string() : network->ssid;
    drawing.CenteredWrappedText(
        &draw, drawing.Regular(), 15.0f, 205.0f, 595.0f, 278.0f, theme::MutedText,
        ("Het opgeslagen wachtwoord voor " + name + " wordt verwijderd.").c_str());
    if (network != nullptr && network->connected) {
        drawing.CenteredText(&draw, drawing.Regular(), 14.0f, 321.0f, theme::RedText,
                             "Hierdoor kan de verbinding direct wegvallen.");
    }
    drawing.ActionButton(&draw, {205, 354}, {385, 408}, "Annuleren",
                         UiControl::AdminNetworkCancel, ButtonVariant::Secondary,
                         VectorIcon::Close);
    drawing.ActionButton(&draw, {415, 354}, {595, 408}, "Vergeten",
                         UiControl::AdminNetworkConfirmForget, ButtonVariant::Primary,
                         VectorIcon::Trash);
}

}  // namespace clockin
