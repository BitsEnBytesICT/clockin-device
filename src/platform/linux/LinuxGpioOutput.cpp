#include "platform/linux/LinuxGpioOutput.hpp"

#include "platform/linux/LinuxDeviceDiscovery.hpp"

#include <chrono>
#include <gpiod.h>
#include <thread>

namespace clockin {

struct LinuxGpioOutput::Impl {
    gpiod_chip* chip = nullptr;
    gpiod_line_request* request = nullptr;
    unsigned int offset = 0;
};

LinuxGpioOutput::LinuxGpioOutput() : impl_(new Impl) {}
LinuxGpioOutput::~LinuxGpioOutput() { Close(); }

bool LinuxGpioOutput::OpenGpioD(unsigned int offset,
                                bool initial_high,
                                std::string& error) {
    if (impl_->request != nullptr) return true;
    const std::string chip_path = linux_platform::FindGpioDChip();
    if (chip_path.empty()) {
        error = "GPIOD character device was not found";
        return false;
    }
    impl_->chip = gpiod_chip_open(chip_path.c_str());
    if (impl_->chip == nullptr) {
        error = linux_platform::ErrnoMessage("Unable to open " + chip_path);
        return false;
    }
    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_config* line_config = gpiod_line_config_new();
    gpiod_request_config* request_config = gpiod_request_config_new();
    if (settings == nullptr || line_config == nullptr || request_config == nullptr) {
        error = "Unable to allocate libgpiod configuration";
    } else {
        gpiod_request_config_set_consumer(request_config, "clockin-mfrc522-reset");
        const bool configured =
            gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT) == 0 &&
            gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_PUSH_PULL) == 0 &&
            gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_DISABLED) == 0 &&
            gpiod_line_settings_set_output_value(
                settings, initial_high ? GPIOD_LINE_VALUE_ACTIVE
                                       : GPIOD_LINE_VALUE_INACTIVE) == 0 &&
            gpiod_line_config_add_line_settings(line_config, &offset, 1, settings) == 0;
        if (configured) {
            impl_->request = gpiod_chip_request_lines(impl_->chip, request_config, line_config);
            if (impl_->request == nullptr) {
                error = linux_platform::ErrnoMessage("Unable to request GPIOD line");
            }
        } else {
            error = linux_platform::ErrnoMessage("Unable to configure GPIOD line");
        }
    }
    if (request_config != nullptr) gpiod_request_config_free(request_config);
    if (line_config != nullptr) gpiod_line_config_free(line_config);
    if (settings != nullptr) gpiod_line_settings_free(settings);
    if (impl_->request == nullptr) {
        Close();
        return false;
    }
    impl_->offset = offset;
    return true;
}

bool LinuxGpioOutput::Set(bool high, std::string& error) {
    if (impl_->request == nullptr) {
        error = "GPIO output requested while the line is closed";
        return false;
    }
    if (gpiod_line_request_set_value(
            impl_->request, impl_->offset,
            high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE) == 0) {
        return true;
    }
    error = linux_platform::ErrnoMessage("Unable to set GPIOD line");
    return false;
}

void LinuxGpioOutput::Close() {
    if (impl_->request != nullptr) gpiod_line_request_release(impl_->request);
    if (impl_->chip != nullptr) gpiod_chip_close(impl_->chip);
    impl_->request = nullptr;
    impl_->chip = nullptr;
}

bool LinuxGpioOutput::IsOpen() const { return impl_->request != nullptr; }

bool PulseMfrc522Reset(LinuxGpioOutput& reset, std::string& error) {
    constexpr unsigned int kResetOffset = 15;
    if (!reset.OpenGpioD(kResetOffset, true, error) || !reset.Set(true, error)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!reset.Set(false, error)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (!reset.Set(true, error)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

}  // namespace clockin
