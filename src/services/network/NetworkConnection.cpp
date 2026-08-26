#include "services/network/NetworkService.hpp"

#include "services/network/NetworkParsing.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace clockin {
namespace {

int ParseNetworkId(const std::map<std::string, std::string>& status) {
    const auto item = status.find("id");
    if (item == status.end()) return -1;
    char* end = nullptr;
    const long value = std::strtol(item->second.c_str(), &end, 10);
    return end != item->second.c_str() && end != nullptr && *end == '\0'
               ? static_cast<int>(value)
               : -1;
}

}  // namespace

void NetworkService::Connect(Command& command) {
    snapshot_.last_error.clear();
    std::string status_response;
    if (!Wpa("STATUS", status_response)) return;
    const auto previous_status = ParseWpaKeyValues(status_response);
    const int previous_id = ParseNetworkId(previous_status);
    const auto previous_ssid_value = previous_status.find("ssid");
    const std::string previous_ssid = previous_ssid_value == previous_status.end()
                                          ? std::string()
                                          : previous_ssid_value->second;
    int target_id = command.network_id;
    int temporary_id = -1;
    if (command.type == CommandType::ConnectNew) {
        if (!ConfigureNewNetwork(command, target_id)) return;
        temporary_id = target_id;
    }
    if (target_id < 0) {
        SetError("Het opgeslagen Wi-Fi-netwerk bestaat niet meer");
        return;
    }
    snapshot_.state = NetworkState::Associating;
    snapshot_.last_error.clear();
    Publish();
    if (!WpaOk("ENABLE_NETWORK " + std::to_string(target_id)) ||
        !WpaOk("SELECT_NETWORK " + std::to_string(target_id)) ||
        !WaitForAssociation(target_id, command.ssid)) {
        if (!stopping_.load()) {
            SetError("Verbinden mislukt. Controleer het wachtwoord en het signaal.");
            RollBack(previous_id, previous_ssid, temporary_id);
        }
        return;
    }
    snapshot_.state = NetworkState::AcquiringAddress;
    snapshot_.current_ssid = command.ssid;
    Publish();
    if (!RestartDhcp() || !WaitForAddress()) {
        if (!stopping_.load()) {
            SetError("Verbonden met Wi-Fi, maar geen IP-adres ontvangen");
            RollBack(previous_id, previous_ssid, temporary_id);
        }
        return;
    }
    snapshot_.connection_generation++;
    if (temporary_id >= 0) {
        if (!WpaOk("SET_NETWORK " + std::to_string(temporary_id) +
                   " id_str \"clockin_saved\"") ||
            !WpaOk("SAVE_CONFIG")) {
            RefreshStatus();
            snapshot_.last_error = "Verbonden, maar het netwerk kon niet worden onthouden";
            Publish();
            return;
        }
    } else if (!WpaOk("SAVE_CONFIG")) {
        RefreshStatus();
        snapshot_.last_error = "Verbonden, maar de netwerkkeuze kon niet worden onthouden";
        Publish();
        return;
    }
    RefreshStatus();
}

void NetworkService::RollBack(int previous_network_id,
                              const std::string& previous_ssid,
                              int temporary_network_id) {
    snapshot_.state = NetworkState::RollingBack;
    Publish();
    if (temporary_network_id >= 0) {
        WpaOk("REMOVE_NETWORK " + std::to_string(temporary_network_id));
    }
    if (previous_network_id >= 0) {
        WpaOk("ENABLE_NETWORK " + std::to_string(previous_network_id));
        WpaOk("SELECT_NETWORK " + std::to_string(previous_network_id));
        if (!previous_ssid.empty()) {
            WaitForAssociation(previous_network_id, previous_ssid);
        }
    } else {
        WpaOk("ENABLE_NETWORK all");
        WpaOk("REASSOCIATE");
    }
    if (RestartDhcp()) WaitForAddress();
    const std::string failure = snapshot_.last_error;
    RefreshStatus();
    snapshot_.last_error = failure;
    if (!snapshot_.associated) snapshot_.state = NetworkState::Error;
    Publish();
}

bool NetworkService::ConfigureNewNetwork(Command& command, int& network_id) {
    if (command.ssid.empty() || command.ssid.size() > 32 ||
        command.security == WifiSecurity::Unsupported) {
        SetError("Dit Wi-Fi-netwerk wordt niet ondersteund");
        return false;
    }
    if (command.security == WifiSecurity::Personal &&
        !IsValidWifiPassword(command.password)) {
        SetError("Gebruik 8-63 normale tekens of een 64-tekens hexadecimale PSK");
        return false;
    }
    std::string response;
    if (!Wpa("ADD_NETWORK", response)) return false;
    char* end = nullptr;
    const long parsed = std::strtol(response.c_str(), &end, 10);
    if (end == response.c_str() || end == nullptr || *end != '\0' || parsed < 0) {
        SetError("Wi-Fi-profiel maken is mislukt");
        return false;
    }
    network_id = static_cast<int>(parsed);
    const std::string prefix = "SET_NETWORK " + std::to_string(network_id) + " ";
    const auto fail = [this, network_id](const std::string& message) {
        WpaOk("REMOVE_NETWORK " + std::to_string(network_id));
        SetError(message);
        return false;
    };
    if (!WpaOk(prefix + "ssid " + EncodeWpaHex(command.ssid)) ||
        !WpaOk(prefix + "id_str \"clockin_pending\"")) {
        return fail("Wi-Fi-profiel instellen is mislukt");
    }
    if (command.security == WifiSecurity::Open) {
        if (!WpaOk(prefix + "key_mgmt NONE")) {
            return fail("Open Wi-Fi-profiel instellen is mislukt");
        }
    } else if (IsRawWifiPsk(command.password)) {
        if (!WpaOk(prefix + "key_mgmt WPA-PSK") ||
            !WpaOk(prefix + "psk " + command.password)) {
            return fail("WPA-profiel instellen is mislukt");
        }
    } else if (command.supports_sae) {
        if (!WpaOk(prefix + "key_mgmt SAE WPA-PSK") ||
            !WpaOk(prefix + "psk " + QuoteWpaValue(command.password)) ||
            !WpaOk(prefix + "sae_password " + QuoteWpaValue(command.password))) {
            return fail("WPA2/WPA3-profiel instellen is mislukt");
        }
    } else if (!WpaOk(prefix + "key_mgmt WPA-PSK") ||
               !WpaOk(prefix + "psk " + QuoteWpaValue(command.password))) {
        return fail("WPA-profiel instellen is mislukt");
    }
    return true;
}

bool NetworkService::WaitForAssociation(int network_id, const std::string& ssid) {
    for (int attempt = 0; attempt < 50 && !stopping_.load(); ++attempt) {
        std::string response;
        if (!Wpa("STATUS", response)) return false;
        const auto status = ParseWpaKeyValues(response);
        const auto state = status.find("wpa_state");
        const auto current_ssid = status.find("ssid");
        if (state != status.end() && state->second == "COMPLETED" &&
            ParseNetworkId(status) == network_id && current_ssid != status.end() &&
            current_ssid->second == ssid) {
            snapshot_.associated = true;
            snapshot_.current_ssid = ssid;
            Publish();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

bool NetworkService::WaitForAddress() {
    for (int attempt = 0; attempt < 40 && !stopping_.load(); ++attempt) {
        const std::string address = Ipv4Address();
        if (!address.empty()) {
            snapshot_.ipv4_address = address;
            Publish();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

void NetworkService::RemovePendingProfiles() {
    std::string response;
    if (!Wpa("LIST_NETWORKS", response)) return;
    bool removed = false;
    for (const KnownWifiNetwork& network : ParseKnownNetworks(response)) {
        std::string id_str;
        if (Wpa("GET_NETWORK " + std::to_string(network.id) + " id_str", id_str) &&
            (id_str == "clockin_pending" || id_str == "\"clockin_pending\"")) {
            removed = WpaOk("REMOVE_NETWORK " + std::to_string(network.id)) || removed;
        }
    }
    if (removed) {
        WpaOk("ENABLE_NETWORK all");
        WpaOk("SAVE_CONFIG");
        WpaOk("REASSOCIATE");
        RestartDhcp();
    }
}

void NetworkService::DisconnectCurrent() {
    if (!WpaOk("DISCONNECT")) {
        SetError("Wi-Fi-verbinding verbreken is mislukt");
        return;
    }
    snapshot_.associated = false;
    snapshot_.current_ssid.clear();
    snapshot_.ipv4_address.clear();
    snapshot_.state = NetworkState::Disconnected;
    snapshot_.last_error.clear();
    Publish();
}

void NetworkService::ForgetNetwork(int network_id) {
    snapshot_.last_error.clear();
    if (network_id < 0 || !WpaOk("REMOVE_NETWORK " + std::to_string(network_id)) ||
        !WpaOk("ENABLE_NETWORK all") ||
        !WpaOk("SAVE_CONFIG")) {
        SetError("Opgeslagen netwerk vergeten is mislukt");
        return;
    }
    WpaOk("REASSOCIATE");
    RestartDhcp();
    RefreshStatus();
}

std::string NetworkService::Ipv4Address() const {
    ifaddrs* addresses = nullptr;
    if (getifaddrs(&addresses) != 0) return {};
    std::string result;
    for (ifaddrs* current = addresses; current != nullptr; current = current->ifa_next) {
        if (current->ifa_addr == nullptr || current->ifa_name == nullptr ||
            interface_name_ != current->ifa_name ||
            current->ifa_addr->sa_family != AF_INET) continue;
        char text[INET_ADDRSTRLEN]{};
        const auto* address = reinterpret_cast<const sockaddr_in*>(current->ifa_addr);
        if (inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) != nullptr) {
            result = text;
            break;
        }
    }
    freeifaddrs(addresses);
    return result;
}

bool NetworkService::RestartDhcp() const {
    const pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        execl("/bin/systemctl", "systemctl", "restart", "udhcpc-wlan0.service",
              static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    pid_t waited = -1;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}  // namespace clockin
