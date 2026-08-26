#include "services/network/NetworkService.hpp"

#include "services/network/NetworkParsing.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace clockin {
namespace {

using Clock = std::chrono::steady_clock;

bool IsOk(const std::string& response) { return response == "OK"; }

}  // namespace

NetworkService::NetworkService(std::string control_path, std::string interface_name)
    : control_path_(std::move(control_path)), interface_name_(std::move(interface_name)) {}

NetworkService::~NetworkService() { Stop(); }

bool NetworkService::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return true;
    stopping_.store(false);
    try {
        thread_ = std::thread(&NetworkService::ThreadMain, this);
    } catch (...) {
        running_.store(false);
        return false;
    }
    return true;
}

void NetworkService::Stop() {
    if (!running_.exchange(false)) return;
    stopping_.store(true);
    condition_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    for (Command& command : commands_) ClearSecret(command.password);
    commands_.clear();
    snapshots_.clear();
}

bool NetworkService::RequestScan() {
    Command command;
    command.type = CommandType::Scan;
    return Submit(std::move(command));
}

bool NetworkService::ConnectKnown(int network_id, const std::string& ssid) {
    Command command;
    command.type = CommandType::ConnectKnown;
    command.network_id = network_id;
    command.ssid = ssid;
    return Submit(std::move(command));
}

bool NetworkService::ConnectNew(const std::string& ssid,
                                const std::string& password,
                                WifiSecurity security,
                                bool supports_sae) {
    Command command;
    command.type = CommandType::ConnectNew;
    command.ssid = ssid;
    command.password = password;
    command.security = security;
    command.supports_sae = supports_sae;
    return Submit(std::move(command));
}

bool NetworkService::Disconnect() {
    Command command;
    command.type = CommandType::Disconnect;
    return Submit(std::move(command));
}

bool NetworkService::Forget(int network_id, const std::string& ssid) {
    Command command;
    command.type = CommandType::Forget;
    command.network_id = network_id;
    command.ssid = ssid;
    return Submit(std::move(command));
}

bool NetworkService::TryPop(NetworkSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshots_.empty()) return false;
    snapshot = std::move(snapshots_.front());
    snapshots_.pop_front();
    return true;
}

bool NetworkService::Submit(Command command) {
    if (!running_.load() || stopping_.load()) {
        ClearSecret(command.password);
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (commands_.size() >= kMaxCommands) {
        ClearSecret(command.password);
        return false;
    }
    commands_.push_back(std::move(command));
    condition_.notify_one();
    return true;
}

void NetworkService::ThreadMain() {
    EnsureControl();
    if (control_.IsOpen()) {
        RemovePendingProfiles();
        RefreshStatus();
    }
    auto next_status = Clock::now() + std::chrono::seconds(2);
    while (!stopping_.load()) {
        Command command;
        bool have_command = false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait_until(lock, next_status, [this] {
                return stopping_.load() || !commands_.empty();
            });
            if (stopping_.load()) break;
            if (!commands_.empty()) {
                command = std::move(commands_.front());
                commands_.pop_front();
                have_command = true;
            }
        }
        if (have_command) {
            Process(command);
            ClearSecret(command.password);
        } else {
            if (!control_.IsOpen()) EnsureControl();
            if (control_.IsOpen()) RefreshStatus();
        }
        next_status = Clock::now() + std::chrono::seconds(2);
    }
    control_.Close();
}

void NetworkService::Process(Command& command) {
    if (!EnsureControl()) return;
    switch (command.type) {
        case CommandType::Scan: Scan(); break;
        case CommandType::ConnectKnown:
        case CommandType::ConnectNew: Connect(command); break;
        case CommandType::Disconnect: DisconnectCurrent(); break;
        case CommandType::Forget: ForgetNetwork(command.network_id); break;
    }
}

bool NetworkService::EnsureControl() {
    if (control_.IsOpen()) return true;
    std::string error;
    if (!control_.Open(control_path_, error)) {
        snapshot_.control_available = false;
        snapshot_.associated = false;
        snapshot_.state = NetworkState::Unavailable;
        snapshot_.last_error = "Wi-Fi-beheer is niet beschikbaar";
        Publish();
        return false;
    }
    std::string response;
    if (!control_.Command("PING", response, error) || response != "PONG") {
        control_.Close();
        snapshot_.control_available = false;
        snapshot_.state = NetworkState::Unavailable;
        snapshot_.last_error = "Wi-Fi-beheer reageert niet";
        Publish();
        return false;
    }
    snapshot_.control_available = true;
    snapshot_.last_error.clear();
    Publish();
    return true;
}

bool NetworkService::Wpa(const std::string& command, std::string& response) {
    std::string error;
    if (control_.Command(command, response, error)) return true;
    control_.Close();
    SetError("Wi-Fi-beheer reageert niet");
    return false;
}

bool NetworkService::WpaOk(const std::string& command) {
    std::string response;
    return Wpa(command, response) && IsOk(response);
}

void NetworkService::Publish() {
    snapshot_.revision++;
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshots_.size() >= kMaxSnapshots) snapshots_.pop_front();
    snapshots_.push_back(snapshot_);
}

void NetworkService::SetError(const std::string& message) {
    snapshot_.state = NetworkState::Error;
    snapshot_.last_error = message;
    Publish();
}

void NetworkService::RefreshStatus(bool preserve_transient_state) {
    std::string status_response;
    std::string known_response;
    std::string scan_response;
    if (!Wpa("STATUS", status_response) ||
        !Wpa("LIST_NETWORKS", known_response) ||
        !Wpa("SCAN_RESULTS", scan_response)) return;
    const auto status = ParseWpaKeyValues(status_response);
    const auto state = status.find("wpa_state");
    const auto ssid = status.find("ssid");
    snapshot_.control_available = true;
    snapshot_.associated = state != status.end() && state->second == "COMPLETED";
    snapshot_.current_ssid = ssid == status.end() ? std::string() : ssid->second;
    snapshot_.ipv4_address = snapshot_.associated ? Ipv4Address() : std::string();
    snapshot_.networks = ParseWifiScan(scan_response, known_response, status_response);
    if (!preserve_transient_state) {
        snapshot_.state = snapshot_.associated ? NetworkState::Connected
                                               : NetworkState::Disconnected;
    }
    Publish();
}

void NetworkService::Scan() {
    snapshot_.state = NetworkState::Scanning;
    snapshot_.last_error.clear();
    Publish();
    if (!WpaOk("SCAN")) {
        SetError("Zoeken naar Wi-Fi-netwerken is mislukt");
        return;
    }
    for (int index = 0; index < 12 && !stopping_.load(); ++index) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!stopping_.load()) RefreshStatus();
}

void NetworkService::ClearSecret(std::string& value) {
    volatile char* data = value.empty() ? nullptr : &value[0];
    for (std::size_t index = 0; index < value.size(); ++index) data[index] = 0;
    value.clear();
    value.shrink_to_fit();
}

}  // namespace clockin
