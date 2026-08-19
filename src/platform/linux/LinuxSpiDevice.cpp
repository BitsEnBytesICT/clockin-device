#include "platform/linux/LinuxSpiDevice.hpp"

#include "platform/linux/LinuxDeviceDiscovery.hpp"

#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace clockin {
namespace {
constexpr std::uint32_t kSpiSpeedHz = 3000000;
}

LinuxSpiDevice::~LinuxSpiDevice() { Close(); }

bool LinuxSpiDevice::Open(std::string& error) {
    if (descriptor_ >= 0) return true;
    device_path_ = linux_platform::FindRfidSpiDevice();
    if (device_path_.empty()) {
        error = "SPI5 RFID spidev device was not found";
        return false;
    }
    descriptor_ = open(device_path_.c_str(), O_RDWR | O_CLOEXEC);
    if (descriptor_ < 0) {
        error = linux_platform::ErrnoMessage("Unable to open " + device_path_);
        return false;
    }
    std::uint32_t mode = SPI_MODE_0;
    std::uint8_t bits = 8;
    std::uint32_t speed = kSpiSpeedHz;
    if (ioctl(descriptor_, SPI_IOC_WR_MODE32, &mode) < 0 ||
        ioctl(descriptor_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(descriptor_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        error = linux_platform::ErrnoMessage("Unable to configure " + device_path_);
        Close();
        return false;
    }
    return true;
}

void LinuxSpiDevice::Close() {
    if (descriptor_ >= 0) close(descriptor_);
    descriptor_ = -1;
}

bool LinuxSpiDevice::Transfer(const std::uint8_t* transmit,
                              std::uint8_t* receive,
                              std::size_t length,
                              std::string& error) {
    if (descriptor_ < 0) {
        error = "SPI transfer requested while the device is closed";
        return false;
    }
    if (length == 0 || length > 4096) {
        error = "Invalid SPI transfer length";
        return false;
    }
    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<unsigned long>(transmit);
    transfer.rx_buf = reinterpret_cast<unsigned long>(receive);
    transfer.len = static_cast<std::uint32_t>(length);
    transfer.speed_hz = kSpiSpeedHz;
    transfer.bits_per_word = 8;
    if (ioctl(descriptor_, SPI_IOC_MESSAGE(1), &transfer) >= 0) return true;
    error = linux_platform::ErrnoMessage("SPI transfer failed on " + device_path_);
    return false;
}

}  // namespace clockin
