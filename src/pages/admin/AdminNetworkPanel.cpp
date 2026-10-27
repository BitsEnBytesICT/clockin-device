#include "pages/admin/AdminNetworkPanel.hpp"

#include "pages/Page.hpp"
#include "services/network/NetworkParsing.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace clockin {
namespace {

constexpr float kListTop = 263.0f;
constexpr float kListBottom = 479.0f;
constexpr float kRowHeight = 58.0f;

}  // namespace

void AdminNetworkPanel::Reset() {
    mode_ = Mode::List;
    selected_ssid_.clear();
    local_error_.clear();
    keyboard_.Clear();
    dragging_ = false;
    scroll_offset_ = 0.0f;
    mode_changed_at_ = 0.0;
}

void AdminNetworkPanel::SetSnapshot(NetworkSnapshot snapshot) {
    snapshot_ = std::move(snapshot);
    ClampScroll();
    if (!selected_ssid_.empty() && Selected() == nullptr) {
        keyboard_.Clear();
        selected_ssid_.clear();
        mode_ = Mode::List;
    }
}

void AdminNetworkPanel::SetApiStatus(bool known, bool online) {
    api_known_ = known;
    api_online_ = online;
}

void AdminNetworkPanel::ShowCommandError(std::string error) {
    local_error_ = std::move(error);
}

UiControl AdminNetworkPanel::HitTest(const ImVec2& point) const {
    if (mode_ == Mode::Password) return keyboard_.HitTest(point);
    if (mode_ == Mode::ConfirmForget) {
        if (PointInRectangle(point, 205, 354, 180, 54)) return UiControl::AdminNetworkCancel;
        if (PointInRectangle(point, 415, 354, 180, 54)) {
            return UiControl::AdminNetworkConfirmForget;
        }
        return UiControl::None;
    }
    if (mode_ == Mode::Details) {
        if (PointInRectangle(point, 65, 163, 145, 48)) {
            return UiControl::AdminNetworkDetailsBack;
        }
        const WifiNetwork* network = Selected();
        if (network == nullptr || Busy()) return UiControl::None;
        if (network->connected && PointInRectangle(point, 470, 371, 250, 58)) {
            return UiControl::AdminNetworkDisconnect;
        }
        if (!network->connected && network->security != WifiSecurity::Unsupported &&
            PointInRectangle(point, 470, 371, 250, 58)) {
            return UiControl::AdminNetworkConnect;
        }
        if (network->known && PointInRectangle(point, 80, 371, 250, 58)) {
            return UiControl::AdminNetworkForget;
        }
        return UiControl::None;
    }
    if (PointInRectangle(point, 684, 153, 55, 55) && !Busy()) {
        return UiControl::AdminNetworkRefresh;
    }
    return NetworkIndexAt(point) >= 0 ? UiControl::AdminNetworkSelect
                                       : UiControl::None;
}

AdminAction AdminNetworkPanel::Activate(const AppEvent& event) {
    AdminAction action;
    if (mode_ == Mode::Password) {
        if (event.control == UiControl::AdminKeyboardCancel) {
            keyboard_.Clear();
            mode_ = Mode::Details;
            mode_changed_at_ = event.occurred_at;
            return action;
        }
        if (event.control == UiControl::AdminKeyboardConnect) {
            const WifiNetwork* network = Selected();
            if (network == nullptr) return action;
            if (!IsValidWifiPassword(keyboard_.Value())) {
                keyboard_.SetError("Gebruik 8-63 tekens of een hexadecimale PSK.");
                return action;
            }
            action.type = AdminActionType::ConnectNewNetwork;
            action.ssid = network->ssid;
            action.password = keyboard_.Value();
            action.security = network->security;
            action.supports_sae = network->supports_sae;
            keyboard_.Clear();
            mode_ = Mode::Details;
            mode_changed_at_ = event.occurred_at;
            return action;
        }
        keyboard_.Activate(event);
        return action;
    }
    if (event.control == UiControl::AdminNetworkRefresh) {
        local_error_.clear();
        action.type = AdminActionType::ScanNetworks;
    } else if (event.control == UiControl::AdminNetworkSelect) {
        const int index = NetworkIndexAt(event.position);
        if (index >= 0) {
            selected_ssid_ = snapshot_.networks[static_cast<std::size_t>(index)].ssid;
            local_error_.clear();
            mode_ = Mode::Details;
            mode_changed_at_ = event.occurred_at;
        }
    } else if (event.control == UiControl::AdminNetworkDetailsBack) {
        Back(event.occurred_at);
    } else if (event.control == UiControl::AdminNetworkConnect) {
        local_error_.clear();
        const WifiNetwork* network = Selected();
        if (network == nullptr) return action;
        if (network->known) {
            action.type = AdminActionType::ConnectKnownNetwork;
            action.network_id = network->network_id;
            action.ssid = network->ssid;
        } else if (network->security == WifiSecurity::Open) {
            action.type = AdminActionType::ConnectNewNetwork;
            action.ssid = network->ssid;
            action.security = network->security;
        } else if (network->security == WifiSecurity::Personal) {
            KeyboardOptions options;
            options.title = "Wachtwoord voor " + network->ssid;
            keyboard_.Open(std::move(options));
            mode_ = Mode::Password;
            mode_changed_at_ = event.occurred_at;
        }
    } else if (event.control == UiControl::AdminNetworkDisconnect) {
        local_error_.clear();
        action.type = AdminActionType::DisconnectNetwork;
    } else if (event.control == UiControl::AdminNetworkForget) {
        mode_ = Mode::ConfirmForget;
        mode_changed_at_ = event.occurred_at;
    } else if (event.control == UiControl::AdminNetworkCancel) {
        mode_ = Mode::Details;
        mode_changed_at_ = event.occurred_at;
    } else if (event.control == UiControl::AdminNetworkConfirmForget) {
        const WifiNetwork* network = Selected();
        if (network != nullptr) {
            action.type = AdminActionType::ForgetNetwork;
            action.network_id = network->network_id;
            action.ssid = network->ssid;
        }
        mode_ = Mode::List;
        selected_ssid_.clear();
        mode_changed_at_ = event.occurred_at;
    }
    return action;
}

bool AdminNetworkPanel::BeginScroll(const ImVec2& point) {
    dragging_ = mode_ == Mode::List &&
                PointInRectangle(point, 52, kListTop, 696, kListBottom - kListTop);
    return dragging_;
}

void AdminNetworkPanel::ScrollBy(float delta_y) {
    if (!dragging_) return;
    scroll_offset_ += delta_y;
    ClampScroll();
}

bool AdminNetworkPanel::HasSubflow() const { return mode_ != Mode::List; }

void AdminNetworkPanel::Back(double now_seconds) {
    keyboard_.Clear();
    local_error_.clear();
    if (mode_ == Mode::Password || mode_ == Mode::ConfirmForget) {
        mode_ = Mode::Details;
    } else {
        mode_ = Mode::List;
        selected_ssid_.clear();
    }
    mode_changed_at_ = now_seconds;
}

void AdminNetworkPanel::ClearSecrets() { keyboard_.Clear(); }

const WifiNetwork* AdminNetworkPanel::Selected() const {
    const auto item = std::find_if(snapshot_.networks.begin(), snapshot_.networks.end(),
                                   [this](const WifiNetwork& network) {
                                       return network.ssid == selected_ssid_;
                                   });
    return item == snapshot_.networks.end() ? nullptr : &*item;
}

int AdminNetworkPanel::NetworkIndexAt(const ImVec2& point) const {
    if (mode_ != Mode::List || point.x < 52 || point.x > 748 ||
        point.y < kListTop || point.y > kListBottom) return -1;
    const int index = static_cast<int>(
        std::floor((point.y - kListTop - scroll_offset_) / kRowHeight));
    return index >= 0 && index < static_cast<int>(snapshot_.networks.size()) ? index : -1;
}

bool AdminNetworkPanel::Busy() const {
    return snapshot_.state == NetworkState::Scanning ||
           snapshot_.state == NetworkState::Associating ||
           snapshot_.state == NetworkState::AcquiringAddress ||
           snapshot_.state == NetworkState::RollingBack;
}

void AdminNetworkPanel::ClampScroll() {
    const float total = static_cast<float>(snapshot_.networks.size()) * kRowHeight;
    const float maximum = std::max(0.0f, total - (kListBottom - kListTop));
    scroll_offset_ = std::max(-maximum, std::min(0.0f, scroll_offset_));
}

}  // namespace clockin
