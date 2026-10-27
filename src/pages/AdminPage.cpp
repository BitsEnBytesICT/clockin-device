#include "pages/AdminPage.hpp"

#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/PageTransition.hpp"
#include "ui/components/VectorIcons.hpp"

#include <cfloat>
#include <utility>

namespace clockin {
namespace {

struct TabBounds {
    float left;
    float right;
    UiControl control;
    const char* label;
    VectorIcon icon;
};

// The tab row shares the back button's row (y 10..64), so the panels are drawn
// kPanelShift pixels higher than their own coordinates. Panels keep their
// original layout; this page translates their vertices and the touch points.
constexpr float kTabTop = 10.0f;
constexpr float kTabBottom = 64.0f;
constexpr float kPanelShift = 35.0f;

constexpr TabBounds kTabs[] = {
    {10.0f, 160.5f, UiControl::AdminTabRfid, "RFID", VectorIcon::Card},
    {168.5f, 319.0f, UiControl::AdminTabPin, "Pincode", VectorIcon::Pin},
    {327.0f, 477.5f, UiControl::AdminTabNetwork, "Netwerk", VectorIcon::Wifi},
    {485.5f, 636.0f, UiControl::AdminTabApi, "API", VectorIcon::Globe},
};

// Moves everything a panel drew, including clip rectangles it pushed (the
// scrolling Wi-Fi list); commands using the page's outer clip stay as they are.
void TranslatePanel(ImDrawList& draw, int first_vertex, int first_command, float offset_y) {
    for (int index = first_vertex; index < draw.VtxBuffer.Size; ++index) {
        draw.VtxBuffer[index].pos.y += offset_y;
    }
    const ImVec2 outer_min = draw.GetClipRectMin();
    const ImVec2 outer_max = draw.GetClipRectMax();
    for (int index = first_command < 0 ? 0 : first_command;
         index < draw.CmdBuffer.Size; ++index) {
        ImVec4& clip = draw.CmdBuffer[index].ClipRect;
        if (clip.x == outer_min.x && clip.y == outer_min.y &&
            clip.z == outer_max.x && clip.w == outer_max.y) continue;
        clip.y += offset_y;
        clip.w += offset_y;
    }
}

}  // namespace

UiControl AdminPage::HitTest(const ImVec2& point) const {
    if (PointInRectangle(point, 650, 10, 140, 54)) return UiControl::AdminBack;
    if (!ActiveSubflow()) {
        for (const TabBounds& tab : kTabs) {
            if (PointInRectangle(point, tab.left, kTabTop, tab.right - tab.left,
                                 kTabBottom - kTabTop)) {
                return tab.control;
            }
        }
    }
    const ImVec2 panel_point = PanelPoint(point);
    if (tab_ == Tab::Pin) return pin_panel_.HitTest(panel_point);
    if (tab_ == Tab::Network) return network_panel_.HitTest(panel_point);
    if (tab_ == Tab::Api) return api_panel_.HitTest(panel_point);
    return UiControl::None;
}

bool AdminPage::BeginScroll(const ImVec2& point) {
    return tab_ == Tab::Network && network_panel_.BeginScroll(PanelPoint(point));
}

void AdminPage::ScrollBy(float delta_y) {
    if (tab_ == Tab::Network) network_panel_.ScrollBy(delta_y);
}

void AdminPage::EndScroll() {
    if (tab_ == Tab::Network) network_panel_.EndScroll();
}

AdminAction AdminPage::Activate(const AppEvent& event) {
    if (event.control == UiControl::AdminBack) {
        if (tab_ == Tab::Pin && pin_panel_.HasSubflow()) {
            pin_panel_.CancelSubflow(event.occurred_at);
            return {};
        }
        if (tab_ == Tab::Network && network_panel_.HasSubflow()) {
            network_panel_.Back(event.occurred_at);
            return {};
        }
        if (tab_ == Tab::Api && api_panel_.HasSubflow()) {
            api_panel_.Cancel(event.occurred_at);
            return {};
        }
        AdminAction action;
        action.type = AdminActionType::Exit;
        return action;
    }
    if (event.control == UiControl::AdminTabRfid) {
        ChangeTab(Tab::Rfid, event.occurred_at);
        return {};
    }
    if (event.control == UiControl::AdminTabPin) {
        ChangeTab(Tab::Pin, event.occurred_at);
        return {};
    }
    if (event.control == UiControl::AdminTabNetwork) {
        ChangeTab(Tab::Network, event.occurred_at);
        AdminAction action;
        action.type = AdminActionType::ScanNetworks;
        return action;
    }
    if (event.control == UiControl::AdminTabApi) {
        ChangeTab(Tab::Api, event.occurred_at);
        return {};
    }
    AppEvent panel_event = event;
    panel_event.position = PanelPoint(event.position);
    if (tab_ == Tab::Pin) return pin_panel_.Activate(panel_event);
    if (tab_ == Tab::Network) return network_panel_.Activate(panel_event);
    if (tab_ == Tab::Api) return api_panel_.Activate(panel_event);
    return {};
}

void AdminPage::SetUid(std::string uid) { rfid_panel_.SetUid(std::move(uid)); }

void AdminPage::SetNetworkSnapshot(NetworkSnapshot snapshot) {
    network_panel_.SetSnapshot(std::move(snapshot));
}

void AdminPage::SetApiStatus(bool known, bool online) {
    network_panel_.SetApiStatus(known, online);
    api_panel_.SetApiStatus(known, online);
}

void AdminPage::SetApiUrl(std::string url) { api_panel_.SetCurrentUrl(std::move(url)); }

void AdminPage::CompletePinSave(bool succeeded,
                                const std::string& message,
                                double now_seconds) {
    pin_panel_.CompleteSave(succeeded, message, now_seconds);
}

void AdminPage::CompleteApiUrlSave(bool succeeded,
                                   const std::string& message,
                                   double now_seconds) {
    api_panel_.CompleteSave(succeeded, message, now_seconds);
}

void AdminPage::ShowNetworkError(std::string message) {
    network_panel_.ShowCommandError(std::move(message));
}

void AdminPage::Reset(bool pin_persistable, double now_seconds) {
    tab_ = Tab::Rfid;
    tab_changed_at_ = now_seconds;
    rfid_panel_.Reset();
    pin_panel_.Reset(pin_persistable);
    network_panel_.Reset();
    api_panel_.Reset();
}

void AdminPage::ClearSecrets() {
    pin_panel_.ClearSecrets();
    network_panel_.ClearSecrets();
    api_panel_.Cancel(0.0);
}

void AdminPage::ChangeTab(Tab tab, double now_seconds) {
    if (tab_ == tab) return;
    pin_panel_.ClearSecrets();
    network_panel_.ClearSecrets();
    api_panel_.Cancel(now_seconds);
    tab_ = tab;
    tab_changed_at_ = now_seconds;
}

bool AdminPage::ActiveSubflow() const {
    return (tab_ == Tab::Pin && pin_panel_.HasSubflow()) ||
           (tab_ == Tab::Network && network_panel_.HasSubflow()) ||
           (tab_ == Tab::Api && api_panel_.HasSubflow());
}

bool AdminPage::KeyboardOpen() const {
    return (tab_ == Tab::Network && network_panel_.KeyboardOpen()) ||
           (tab_ == Tab::Api && api_panel_.KeyboardOpen());
}

ImVec2 AdminPage::PanelPoint(const ImVec2& point) const {
    // The full-screen keyboard is laid out in screen coordinates.
    if (KeyboardOpen()) return point;
    return {point.x, point.y + kPanelShift};
}

void AdminPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.ActionButton(&draw, {650, 10}, {790, 64}, "Terug",
                         UiControl::AdminBack, ButtonVariant::Secondary,
                         VectorIcon::Back);

    const bool keyboard_open = KeyboardOpen();
    if (!keyboard_open) {
        constexpr float kIconSize = 22.0f;
        constexpr float kGap = 8.0f;
        constexpr float kLabelSize = 16.0f;
        const float center_y = (kTabTop + kTabBottom) * 0.5f;
        for (const TabBounds& bounds : kTabs) {
            const bool active =
                static_cast<int>(tab_) == static_cast<int>(&bounds - kTabs);
            draw.AddRectFilled({bounds.left, kTabTop}, {bounds.right, kTabBottom},
                               active ? theme::PressedSurface : theme::White, 11.0f);
            draw.AddRect({bounds.left, kTabTop}, {bounds.right, kTabBottom},
                         active ? theme::Blue : theme::Border, 11.0f, 0,
                         active ? 2.0f : 1.0f);
            const ImU32 color = active ? theme::Blue : theme::MutedText;
            const ImVec2 label_size = drawing.SemiBold()->CalcTextSizeA(
                kLabelSize, FLT_MAX, 0.0f, bounds.label);
            const float start_x =
                (bounds.left + bounds.right - (kIconSize + kGap + label_size.x)) * 0.5f;
            DrawVectorIcon(draw, bounds.icon, {start_x + kIconSize * 0.5f, center_y},
                           kIconSize, color);
            drawing.Text(&draw, drawing.SemiBold(), kLabelSize,
                         {start_x + kIconSize + kGap, center_y - label_size.y * 0.5f},
                         color, bounds.label);
        }
    }

    const int first_panel_vertex = draw.VtxBuffer.Size;
    const int first_panel_command = draw.CmdBuffer.Size - 1;
    if (tab_ == Tab::Rfid) rfid_panel_.Render(draw, drawing);
    else if (tab_ == Tab::Pin) pin_panel_.Render(draw, drawing);
    else if (tab_ == Tab::Network) network_panel_.Render(draw, drawing);
    else api_panel_.Render(draw, drawing);
    if (!keyboard_open) {
        TranslatePanel(draw, first_panel_vertex, first_panel_command, -kPanelShift);
    }
    ApplyPageTransition(
        draw, first_panel_vertex,
        EvaluatePageTransition(drawing.NowSeconds() - tab_changed_at_));
}

}  // namespace clockin
