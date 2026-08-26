#pragma once

#include <string>

namespace clockin {

class WpaControlSocket {
public:
    WpaControlSocket() = default;
    ~WpaControlSocket();

    WpaControlSocket(const WpaControlSocket&) = delete;
    WpaControlSocket& operator=(const WpaControlSocket&) = delete;

    bool Open(const std::string& remote_path, std::string& error);
    void Close();
    bool Command(const std::string& command,
                 std::string& response,
                 std::string& error,
                 int timeout_ms = 2000);
    bool IsOpen() const { return descriptor_ >= 0; }

private:
    int descriptor_ = -1;
    std::string local_path_;
};

}  // namespace clockin
