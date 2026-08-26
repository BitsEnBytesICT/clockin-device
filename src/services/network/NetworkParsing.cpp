#include "services/network/NetworkParsing.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace clockin {
namespace {

std::vector<std::string> SplitTabs(const std::string& line) {
    std::vector<std::string> values;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t end = line.find('\t', start);
        values.push_back(line.substr(start, end == std::string::npos
                                               ? std::string::npos
                                               : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return values;
}

bool Contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

WifiSecurity DetectSecurity(const std::string& flags, bool& supports_sae) {
    supports_sae = Contains(flags, "SAE");
    const bool enterprise = Contains(flags, "EAP") || Contains(flags, "IEEE8021X");
    const bool legacy_wep = Contains(flags, "WEP");
    const bool personal = Contains(flags, "PSK") || supports_sae;
    const bool enhanced_open = Contains(flags, "OWE");
    if (enterprise || legacy_wep || enhanced_open) return WifiSecurity::Unsupported;
    if (personal) return WifiSecurity::Personal;
    return WifiSecurity::Open;
}

int ParseInteger(const std::string& value, int fallback) {
    char* end = nullptr;
    const long number = std::strtol(value.c_str(), &end, 10);
    return end != value.c_str() && end != nullptr && *end == '\0'
               ? static_cast<int>(number)
               : fallback;
}

int HexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

}  // namespace

std::map<std::string, std::string> ParseWpaKeyValues(const std::string& response) {
    std::map<std::string, std::string> values;
    std::istringstream input(response);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        values[line.substr(0, separator)] = DecodeWpaText(line.substr(separator + 1));
    }
    return values;
}

std::vector<KnownWifiNetwork> ParseKnownNetworks(const std::string& response) {
    std::vector<KnownWifiNetwork> networks;
    std::istringstream input(response);
    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            first = false;
            if (line.find("network id") != std::string::npos) continue;
        }
        const std::vector<std::string> fields = SplitTabs(line);
        if (fields.size() < 2) continue;
        const int id = ParseInteger(fields[0], -1);
        if (id < 0) continue;
        KnownWifiNetwork network;
        network.id = id;
        network.ssid = DecodeWpaText(fields[1]);
        network.current = fields.size() >= 4 && Contains(fields[3], "CURRENT");
        networks.push_back(std::move(network));
    }
    return networks;
}

std::vector<WifiNetwork> ParseWifiScan(const std::string& scan_response,
                                       const std::string& known_response,
                                       const std::string& status_response) {
    const std::vector<KnownWifiNetwork> known = ParseKnownNetworks(known_response);
    const auto status = ParseWpaKeyValues(status_response);
    const auto state = status.find("wpa_state");
    const auto current = status.find("ssid");
    const bool associated = state != status.end() && state->second == "COMPLETED";
    const std::string current_ssid = current == status.end() ? std::string() : current->second;

    std::map<std::string, WifiNetwork> strongest;
    std::istringstream input(scan_response);
    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            first = false;
            if (line.find("bssid") != std::string::npos) continue;
        }
        const std::vector<std::string> fields = SplitTabs(line);
        if (fields.size() < 5) continue;
        const std::string ssid = DecodeWpaText(fields[4]);
        if (ssid.empty()) continue;
        WifiNetwork candidate;
        candidate.bssid = fields[0];
        candidate.signal_dbm = ParseInteger(fields[2], -100);
        candidate.ssid = ssid;
        candidate.security = DetectSecurity(fields[3], candidate.supports_sae);
        for (const KnownWifiNetwork& saved : known) {
            if (saved.ssid == ssid) {
                candidate.known = true;
                candidate.network_id = saved.id;
                candidate.connected = saved.current;
                break;
            }
        }
        if (!candidate.connected) {
            candidate.connected = associated && candidate.ssid == current_ssid;
        }
        auto existing = strongest.find(ssid);
        if (existing == strongest.end() || candidate.signal_dbm > existing->second.signal_dbm) {
            strongest[ssid] = std::move(candidate);
        }
    }

    std::vector<WifiNetwork> result;
    result.reserve(strongest.size());
    for (auto& item : strongest) result.push_back(std::move(item.second));
    std::sort(result.begin(), result.end(), [](const WifiNetwork& first,
                                               const WifiNetwork& second) {
        if (first.connected != second.connected) return first.connected;
        if (first.known != second.known) return first.known;
        if (first.signal_dbm != second.signal_dbm) {
            return first.signal_dbm > second.signal_dbm;
        }
        return first.ssid < second.ssid;
    });
    return result;
}

std::string DecodeWpaText(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 >= value.size()) {
            decoded.push_back(value[index]);
            continue;
        }
        const char escaped = value[++index];
        if (escaped == 'n') decoded.push_back('\n');
        else if (escaped == 'r') decoded.push_back('\r');
        else if (escaped == 't') decoded.push_back('\t');
        else if (escaped == '\\') decoded.push_back('\\');
        else if (escaped == 'x' && index + 2 < value.size()) {
            const int high = HexDigit(value[index + 1]);
            const int low = HexDigit(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
            } else {
                decoded.push_back(escaped);
            }
        } else {
            decoded.push_back(escaped);
        }
    }
    return decoded;
}

std::string EncodeWpaHex(const std::string& value) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size() * 2);
    for (unsigned char character : value) {
        encoded.push_back(kHex[(character >> 4) & 0x0F]);
        encoded.push_back(kHex[character & 0x0F]);
    }
    return encoded;
}

bool IsValidWifiPassword(const std::string& password) {
    if (IsRawWifiPsk(password)) return true;
    if (password.size() < 8 || password.size() > 63) return false;
    return std::all_of(password.begin(), password.end(), [](unsigned char character) {
        return character >= 0x20 && character <= 0x7E;
    });
}

bool IsRawWifiPsk(const std::string& password) {
    return password.size() == 64 &&
           std::all_of(password.begin(), password.end(), [](unsigned char character) {
               return std::isxdigit(character) != 0;
           });
}

std::string QuoteWpaValue(const std::string& value) {
    std::string quoted = "\"";
    quoted.reserve(value.size() + 2);
    for (char character : value) {
        if (character == '\\' || character == '"') quoted.push_back('\\');
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

}  // namespace clockin
