#pragma once

#include "services/network/INetworkService.hpp"
#include "services/network/WpaControlSocket.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace clockin {

class NetworkService final : public INetworkService {
public:
    explicit NetworkService(
        std::string control_path = "/run/wpa_supplicant/wlan0",
        std::string interface_name = "wlan0");
    ~NetworkService() override;

    bool Start() override;
    void Stop() override;
    bool RequestScan() override;
    bool ConnectKnown(int network_id, const std::string& ssid) override;
    bool ConnectNew(const std::string& ssid,
                    const std::string& password,
                    WifiSecurity security,
                    bool supports_sae) override;
    bool Disconnect() override;
    bool Forget(int network_id, const std::string& ssid) override;
    bool TryPop(NetworkSnapshot& snapshot) override;

private:
    enum class CommandType { Scan, ConnectKnown, ConnectNew, Disconnect, Forget };

    struct Command {
        CommandType type = CommandType::Scan;
        int network_id = -1;
        std::string ssid;
        std::string password;
        WifiSecurity security = WifiSecurity::Unsupported;
        bool supports_sae = false;
    };

    static constexpr std::size_t kMaxCommands = 4;
    static constexpr std::size_t kMaxSnapshots = 8;

    std::string control_path_;
    std::string interface_name_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Command> commands_;
    std::deque<NetworkSnapshot> snapshots_;
    NetworkSnapshot snapshot_;
    WpaControlSocket control_;

    bool Submit(Command command);
    void ThreadMain();
    void Process(Command& command);
    bool EnsureControl();
    bool Wpa(const std::string& command, std::string& response);
    bool WpaOk(const std::string& command);
    void Publish();
    void SetError(const std::string& message);
    void RefreshStatus(bool preserve_transient_state = false);
    void Scan();
    void Connect(Command& command);
    void RollBack(int previous_network_id,
                  const std::string& previous_ssid,
                  int temporary_network_id);
    bool ConfigureNewNetwork(Command& command, int& network_id);
    bool WaitForAssociation(int network_id, const std::string& ssid);
    bool WaitForAddress();
    void RemovePendingProfiles();
    void DisconnectCurrent();
    void ForgetNetwork(int network_id);
    std::string Ipv4Address() const;
    bool RestartDhcp() const;
    static void ClearSecret(std::string& value);
};

}  // namespace clockin
