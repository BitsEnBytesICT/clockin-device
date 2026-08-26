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

constexpr TabBounds kTabs[] = {
    {65.0f, 275.0f, UiControl::AdminTabRfid, "RFID uitlezen", VectorIcon::Card},
    {295.0f, 505.0f, UiControl::AdminTabPin, "Pincode", VectorIcon::Pin},
    {525.0f, 735.0f, UiControl::AdminTabNetwork, "Netwerk", VectorIcon::Wifi},
};

}  // namespace

UiControl AdminPage::HitTest(const ImVec2& point) const {
    if (PointInRectangle(point, 650, 10, 140, 54)) return UiControl::AdminBack;
    if (!ActiveSubflow()) {
        for (const TabBounds& tab : kTabs) {
            if (PointInRectangle(point, tab.left, 82, tab.right - tab.left, 52)) {
                return tab.control;
            }
        }
    }
    if (tab_ == Tab::Pin) return pin_panel_.HitTest(point);
    if (tab_ == Tab::Network) return network_panel_.HitTest(point);
    return UiControl::None;
}

bool AdminPage::BeginScroll(const ImVec2& point) {
    return tab_ == Tab::Network && network_panel_.BeginScroll(point);
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
    if (tab_ == Tab::Pin) return pin_panel_.Activate(event);
    if (tab_ == Tab::Network) return network_panel_.Activate(event);
    return {};
}

void AdminPage::SetUid(std::string uid) { rfid_panel_.SetUid(std::move(uid)); }

void AdminPage::SetNetworkSnapshot(NetworkSnapshot snapshot) {
    network_panel_.SetSnapshot(std::move(snapshot));
}

void AdminPage::SetApiStatus(bool known, bool online) {
    network_panel_.SetApiStatus(known, online);
}

void AdminPage::CompletePinSave(bool succeeded,
                                const std::string& message,
                                double now_seconds) {
    pin_panel_.CompleteSave(succeeded, message, now_seconds);
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
}

void AdminPage::ClearSecrets() {
    pin_panel_.ClearSecrets();
    network_panel_.ClearSecrets();
}

void AdminPage::ChangeTab(Tab tab, double now_seconds) {
    if (tab_ == tab) return;
    pin_panel_.ClearSecrets();
    network_panel_.ClearSecrets();
    tab_ = tab;
    tab_changed_at_ = now_seconds;
}

bool AdminPage::ActiveSubflow() const {
    return (tab_ == Tab::Pin && pin_panel_.HasSubflow()) ||
           (tab_ == Tab::Network && network_panel_.HasSubflow());
}

void AdminPage::Render(ImDrawList& draw, PageDrawing& drawing) const {
    drawing.Text(&draw, drawing.SemiBold(), 27.0f, {40, 25}, theme::Text,
                 "Beheer");
    drawing.Text(&draw, drawing.Regular(), 14.0f, {40, 56}, theme::MutedText,
                 "Apparaatinstellingen");
    drawing.ActionButton(&draw, {650, 10}, {790, 64}, "Terug",
                         UiControl::AdminBack, ButtonVariant::Secondary,
                         VectorIcon::Back);

    const bool keyboard_open = tab_ == Tab::Network && network_panel_.KeyboardOpen();
    if (!keyboard_open) {
        for (std::size_t index = 0; index < 3; ++index) {
            const TabBounds& bounds = kTabs[index];
            const bool active = static_cast<int>(tab_) == static_cast<int>(index);
            draw.AddRectFilled({bounds.left, 82}, {bounds.right, 134},
                               active ? theme::PressedSurface : theme::White, 11.0f);
            draw.AddRect({bounds.left, 82}, {bounds.right, 134},
                         active ? theme::Blue : theme::Border, 11.0f, 0,
                         active ? 2.0f : 1.0f);
            DrawVectorIcon(draw, bounds.icon, {bounds.left + 34.0f, 108.0f}, 24.0f,
                           active ? theme::Blue : theme::MutedText);
            const ImVec2 label_size = drawing.SemiBold()->CalcTextSizeA(
                16.0f, FLT_MAX, 0.0f, bounds.label);
            drawing.Text(&draw, drawing.SemiBold(), 16.0f,
                         {(bounds.left + bounds.right + 22.0f - label_size.x) * 0.5f,
                          99.0f},
                         active ? theme::Blue : theme::MutedText, bounds.label);
        }
    }

    const int first_panel_vertex = draw.VtxBuffer.Size;
    if (tab_ == Tab::Rfid) rfid_panel_.Render(draw, drawing);
    else if (tab_ == Tab::Pin) pin_panel_.Render(draw, drawing);
    else network_panel_.Render(draw, drawing);
    ApplyPageTransition(
        draw, first_panel_vertex,
        EvaluatePageTransition(drawing.NowSeconds() - tab_changed_at_));
}

}  // namespace clockin
