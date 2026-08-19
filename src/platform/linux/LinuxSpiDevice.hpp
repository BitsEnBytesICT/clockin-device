#pragma once

#include "services/hardware/rfid/SpiTransport.hpp"

#include <string>

namespace clockin {

class LinuxSpiDevice final : public SpiTransport {
public:
    LinuxSpiDevice() = default;
    ~LinuxSpiDevice() override;
    bool Open(std::string& error);
    void Close();
    bool IsOpen() const { return descriptor_ >= 0; }
    const std::string& DevicePath() const { return device_path_; }
    bool Transfer(const std::uint8_t* transmit,
                  std::uint8_t* receive,
                  std::size_t length,
                  std::string& error) override;

private:
    int descriptor_ = -1;
    std::string device_path_;
};

}  // namespace clockin
