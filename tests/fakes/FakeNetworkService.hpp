#pragma once

#include "services/network/INetworkService.hpp"

#include <deque>

namespace clockin::test {

class FakeNetworkService final : public INetworkService {
public:
    bool Start() override { return started = true; }
    void Stop() override { started = false; }
    bool RequestScan() override { ++scan_requests; return accepting; }
    bool ConnectKnown(int network_id, const std::string& ssid) override {
        last_network_id = network_id;
        last_ssid = ssid;
        return accepting;
    }
    bool ConnectNew(const std::string& ssid,
                    const std::string& password,
                    WifiSecurity security,
                    bool supports_sae) override {
        last_ssid = ssid;
        last_password = password;
        last_security = security;
        last_supports_sae = supports_sae;
        return accepting;
    }
    bool Disconnect() override { ++disconnect_requests; return accepting; }
    bool Forget(int network_id, const std::string& ssid) override {
        last_network_id = network_id;
        last_ssid = ssid;
        ++forget_requests;
        return accepting;
    }
    bool TryPop(NetworkSnapshot& snapshot) override {
        if (snapshots.empty()) return false;
        snapshot = snapshots.front();
        snapshots.pop_front();
        return true;
    }

    bool started = false;
    bool accepting = true;
    int scan_requests = 0;
    int disconnect_requests = 0;
    int forget_requests = 0;
    int last_network_id = -1;
    std::string last_ssid;
    std::string last_password;
    WifiSecurity last_security = WifiSecurity::Unsupported;
    bool last_supports_sae = false;
    std::deque<NetworkSnapshot> snapshots;
};

}  // namespace clockin::test
