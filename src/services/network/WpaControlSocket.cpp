#include "services/network/WpaControlSocket.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace clockin {
namespace {

bool FillAddress(const std::string& path, sockaddr_un& address, std::string& error) {
    if (path.empty() || path.size() >= sizeof(address.sun_path)) {
        error = "Ongeldig wpa_supplicant socketpad";
        return false;
    }
    address = sockaddr_un{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return true;
}

}  // namespace

WpaControlSocket::~WpaControlSocket() { Close(); }

bool WpaControlSocket::Open(const std::string& remote_path, std::string& error) {
    Close();
    if (mkdir("/run/clockin-device", 0700) != 0 && errno != EEXIST) {
        error = std::string("Runtime-map maken mislukt: ") + std::strerror(errno);
        return false;
    }
    descriptor_ = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (descriptor_ < 0) {
        error = std::string("Netwerksocket openen mislukt: ") + std::strerror(errno);
        return false;
    }
    local_path_ = "/run/clockin-device/wpa-" + std::to_string(getpid()) + "-" +
                  std::to_string(descriptor_);
    sockaddr_un local{};
    sockaddr_un remote{};
    if (!FillAddress(local_path_, local, error) ||
        !FillAddress(remote_path, remote, error)) {
        Close();
        return false;
    }
    unlink(local_path_.c_str());
    if (bind(descriptor_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
        error = std::string("Lokale netwerksocket koppelen mislukt: ") +
                std::strerror(errno);
        Close();
        return false;
    }
    chmod(local_path_.c_str(), 0600);
    if (connect(descriptor_, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote)) != 0) {
        error = std::string("wpa_supplicant is niet bereikbaar: ") +
                std::strerror(errno);
        Close();
        return false;
    }
    return true;
}

void WpaControlSocket::Close() {
    if (descriptor_ >= 0) close(descriptor_);
    descriptor_ = -1;
    if (!local_path_.empty()) unlink(local_path_.c_str());
    local_path_.clear();
}

bool WpaControlSocket::Command(const std::string& command,
                               std::string& response,
                               std::string& error,
                               int timeout_ms) {
    response.clear();
    if (descriptor_ < 0) {
        error = "wpa_supplicant controlsocket is gesloten";
        return false;
    }
    const ssize_t sent = send(descriptor_, command.data(), command.size(), MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(command.size())) {
        error = std::string("Netwerkopdracht verzenden mislukt: ") + std::strerror(errno);
        return false;
    }
    pollfd descriptor{descriptor_, POLLIN, 0};
    int poll_result = 0;
    do {
        poll_result = poll(&descriptor, 1, timeout_ms);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result <= 0 || (descriptor.revents & POLLIN) == 0) {
        error = poll_result == 0 ? "wpa_supplicant reageert niet"
                                 : std::string("Netwerkantwoord mislukt: ") +
                                       std::strerror(errno);
        return false;
    }
    char buffer[65536];
    const ssize_t count = recv(descriptor_, buffer, sizeof(buffer), 0);
    if (count < 0) {
        error = std::string("Netwerkantwoord lezen mislukt: ") + std::strerror(errno);
        return false;
    }
    response.assign(buffer, static_cast<std::size_t>(count));
    while (!response.empty() &&
           (response.back() == '\n' || response.back() == '\r')) {
        response.pop_back();
    }
    return true;
}

}  // namespace clockin
