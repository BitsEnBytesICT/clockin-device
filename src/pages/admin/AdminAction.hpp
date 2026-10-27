#pragma once

#include "services/network/NetworkTypes.hpp"

#include <string>

namespace clockin {

enum class AdminActionType {
    None,
    Exit,
    ScanNetworks,
    ConnectKnownNetwork,
    ConnectNewNetwork,
    DisconnectNetwork,
    ForgetNetwork,
    SavePin,
    SaveApiUrl
};

struct AdminAction {
    AdminActionType type = AdminActionType::None;
    int network_id = -1;
    std::string ssid;
    std::string password;
    std::string pin;
    std::string api_url;
    WifiSecurity security = WifiSecurity::Unsupported;
    bool supports_sae = false;
};

}  // namespace clockin
