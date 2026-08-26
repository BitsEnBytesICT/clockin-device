#include "services/network/NetworkTypes.hpp"

namespace clockin {

const char* NetworkStateLabel(NetworkState state) {
    switch (state) {
        case NetworkState::Unavailable: return "Niet beschikbaar";
        case NetworkState::Disconnected: return "Niet verbonden";
        case NetworkState::Scanning: return "Netwerken zoeken";
        case NetworkState::Associating: return "Verbinden";
        case NetworkState::AcquiringAddress: return "IP-adres ophalen";
        case NetworkState::Connected: return "Verbonden";
        case NetworkState::RollingBack: return "Vorige verbinding herstellen";
        case NetworkState::Error: return "Verbindingsfout";
    }
    return "Onbekend";
}

const char* WifiSecurityLabel(WifiSecurity security, bool supports_sae) {
    switch (security) {
        case WifiSecurity::Open: return "Open";
        case WifiSecurity::Personal: return supports_sae ? "WPA2/WPA3" : "WPA/WPA2";
        case WifiSecurity::Unsupported: return "Niet ondersteund";
    }
    return "Onbekend";
}

}  // namespace clockin
