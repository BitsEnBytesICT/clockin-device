#pragma once

#include <string>
#include <vector>

namespace clockin {

enum class WifiSecurity {
    Open,
    Personal,
    Unsupported
};

enum class NetworkState {
    Unavailable,
    Disconnected,
    Scanning,
    Associating,
    AcquiringAddress,
    Connected,
    RollingBack,
    Error
};

struct WifiNetwork {
    std::string ssid;
    std::string bssid;
    int signal_dbm = -100;
    int network_id = -1;
    WifiSecurity security = WifiSecurity::Unsupported;
    bool supports_sae = false;
    bool known = false;
    bool connected = false;
};

struct NetworkSnapshot {
    NetworkState state = NetworkState::Unavailable;
    bool control_available = false;
    bool associated = false;
    std::string current_ssid;
    std::string ipv4_address;
    std::string last_error;
    std::vector<WifiNetwork> networks;
    unsigned long long revision = 0;
    unsigned long long connection_generation = 0;
};

const char* NetworkStateLabel(NetworkState state);
const char* WifiSecurityLabel(WifiSecurity security, bool supports_sae);

}  // namespace clockin
