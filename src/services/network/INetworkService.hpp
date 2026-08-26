#pragma once

#include "services/network/NetworkTypes.hpp"

#include <string>

namespace clockin {

class INetworkService {
public:
    virtual ~INetworkService() = default;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool RequestScan() = 0;
    virtual bool ConnectKnown(int network_id, const std::string& ssid) = 0;
    virtual bool ConnectNew(const std::string& ssid,
                            const std::string& password,
                            WifiSecurity security,
                            bool supports_sae) = 0;
    virtual bool Disconnect() = 0;
    virtual bool Forget(int network_id, const std::string& ssid) = 0;
    virtual bool TryPop(NetworkSnapshot& snapshot) = 0;
};

}  // namespace clockin
