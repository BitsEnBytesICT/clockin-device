#include "platform/linux/LinuxDeviceDiscovery.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

namespace clockin::linux_platform {
namespace {

constexpr const char* kSpiDeviceTreePath = "spi@44009000/rfid@0";
constexpr const char* kGpioDeviceTreePath = "gpio@50005000";
constexpr const char* kTim16DeviceTreePath = "timer@44007000";

std::string FindGpioChip(const char* device_tree_fragment) {
    for (const std::string& chip : DirectoryEntries("/sys/bus/gpio/devices")) {
        if (chip.compare(0, 8, "gpiochip") != 0) continue;
        const std::string sysfs = "/sys/bus/gpio/devices/" + chip;
        if (ResolvePath(sysfs + "/of_node").find(device_tree_fragment) == std::string::npos) {
            continue;
        }
        const std::string candidate = "/dev/" + chip;
        if (PathExists(candidate)) return candidate;
    }
    return {};
}

}  // namespace

bool PathExists(const std::string& path) {
    struct stat information {};
    return stat(path.c_str(), &information) == 0;
}

std::string ResolvePath(const std::string& path) {
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved) == nullptr) return {};
    return resolved;
}

std::vector<std::string> DirectoryEntries(const std::string& path) {
    std::vector<std::string> entries;
    DIR* directory = opendir(path.c_str());
    if (directory == nullptr) return entries;
    while (dirent* entry = readdir(directory)) {
        if (entry->d_name[0] != '.') entries.emplace_back(entry->d_name);
    }
    closedir(directory);
    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string ErrnoMessage(const std::string& operation) {
    return operation + ": " + std::strerror(errno);
}

bool WriteAttribute(const std::string& path,
                    const std::string& value,
                    std::string& error,
                    int* error_number) {
    const int descriptor = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) {
        if (error_number != nullptr) *error_number = errno;
        error = ErrnoMessage("Unable to open " + path);
        return false;
    }
    std::size_t offset = 0;
    while (offset < value.size()) {
        const ssize_t written = write(descriptor, value.data() + offset, value.size() - offset);
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        const int saved_error = written < 0 ? errno : EIO;
        close(descriptor);
        errno = saved_error;
        if (error_number != nullptr) *error_number = saved_error;
        error = ErrnoMessage("Unable to write " + path);
        return false;
    }
    if (close(descriptor) != 0) {
        if (error_number != nullptr) *error_number = errno;
        error = ErrnoMessage("Unable to close " + path);
        return false;
    }
    if (error_number != nullptr) *error_number = 0;
    return true;
}

std::string FindRfidSpiDevice() {
    const char* configured = std::getenv("CLOCKIN_SPI_DEVICE");
    if (configured != nullptr && configured[0] != '\0') return configured;
    for (const std::string& device : DirectoryEntries("/sys/bus/spi/devices")) {
        const std::string sysfs = "/sys/bus/spi/devices/" + device;
        if (ResolvePath(sysfs + "/of_node").find(kSpiDeviceTreePath) == std::string::npos) {
            continue;
        }
        const std::string bus_and_chip = device.compare(0, 3, "spi") == 0
                                             ? device.substr(3)
                                             : device;
        const std::string candidate = "/dev/spidev" + bus_and_chip;
        if (PathExists(candidate)) return candidate;
    }
    std::string only_device;
    std::size_t count = 0;
    for (const std::string& entry : DirectoryEntries("/dev")) {
        if (entry.compare(0, 7, "spidev") != 0) continue;
        only_device = "/dev/" + entry;
        ++count;
    }
    return count == 1 ? only_device : std::string();
}

std::string FindGpioDChip() { return FindGpioChip(kGpioDeviceTreePath); }

std::string FindTim16PwmChip() {
    for (const std::string& chip : DirectoryEntries("/sys/class/pwm")) {
        if (chip.compare(0, 7, "pwmchip") != 0) continue;
        const std::string sysfs = "/sys/class/pwm/" + chip;
        const std::string device_tree = ResolvePath(sysfs + "/device/of_node");
        const std::string device = ResolvePath(sysfs + "/device");
        if (device_tree.find(kTim16DeviceTreePath) != std::string::npos ||
            device.find("44007000") != std::string::npos) {
            return sysfs;
        }
    }
    return {};
}

}  // namespace clockin::linux_platform
