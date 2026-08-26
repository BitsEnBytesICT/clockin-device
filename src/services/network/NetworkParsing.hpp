#pragma once

#include "services/network/NetworkTypes.hpp"

#include <map>
#include <string>
#include <vector>

namespace clockin {

struct KnownWifiNetwork {
    int id = -1;
    std::string ssid;
    bool current = false;
};

std::map<std::string, std::string> ParseWpaKeyValues(const std::string& response);
std::vector<KnownWifiNetwork> ParseKnownNetworks(const std::string& response);
std::vector<WifiNetwork> ParseWifiScan(const std::string& scan_response,
                                       const std::string& known_response,
                                       const std::string& status_response);
std::string DecodeWpaText(const std::string& value);
std::string EncodeWpaHex(const std::string& value);
bool IsValidWifiPassword(const std::string& password);
bool IsRawWifiPsk(const std::string& password);
std::string QuoteWpaValue(const std::string& value);

}  // namespace clockin
