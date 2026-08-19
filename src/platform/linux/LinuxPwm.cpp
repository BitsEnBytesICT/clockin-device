#include "platform/linux/LinuxPwm.hpp"

#include "platform/linux/LinuxDeviceDiscovery.hpp"

#include <cerrno>
#include <chrono>
#include <thread>

namespace clockin {

LinuxPwm::~LinuxPwm() { Close(); }

bool LinuxPwm::OpenTim16Channel1(std::string& error) {
    if (!pwm_path_.empty()) return true;
    chip_path_ = linux_platform::FindTim16PwmChip();
    if (chip_path_.empty()) {
        error = "TIM16 PWM chip was not found";
        return false;
    }
    pwm_path_ = chip_path_ + "/pwm0";
    if (linux_platform::PathExists(pwm_path_)) return true;
    int write_error = 0;
    if (!linux_platform::WriteAttribute(chip_path_ + "/export", "0", error, &write_error) &&
        write_error != EBUSY) {
        pwm_path_.clear();
        return false;
    }
    exported_by_us_ = write_error != EBUSY;
    for (int attempt = 0; attempt < 50 && !linux_platform::PathExists(pwm_path_); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (linux_platform::PathExists(pwm_path_)) return true;
    error = "TIM16 PWM channel did not appear after export";
    pwm_path_.clear();
    return false;
}

bool LinuxPwm::Configure(std::uint64_t period_nanoseconds,
                         std::uint64_t duty_nanoseconds,
                         std::string& error) {
    if (pwm_path_.empty()) {
        error = "PWM configuration requested while the channel is closed";
        return false;
    }
    if (period_nanoseconds == 0 || duty_nanoseconds >= period_nanoseconds) {
        error = "Invalid PWM period or duty cycle";
        return false;
    }
    return SetEnabled(false, error) &&
           linux_platform::WriteAttribute(pwm_path_ + "/duty_cycle", "0", error) &&
           linux_platform::WriteAttribute(pwm_path_ + "/period",
                                          std::to_string(period_nanoseconds), error) &&
           linux_platform::WriteAttribute(pwm_path_ + "/duty_cycle",
                                          std::to_string(duty_nanoseconds), error);
}

bool LinuxPwm::SetEnabled(bool enabled, std::string& error) {
    if (pwm_path_.empty()) {
        if (!enabled) return true;
        error = "PWM enable requested while the channel is closed";
        return false;
    }
    if (enabled_ == enabled) return true;
    if (!linux_platform::WriteAttribute(pwm_path_ + "/enable", enabled ? "1" : "0", error)) {
        return false;
    }
    enabled_ = enabled;
    return true;
}

void LinuxPwm::Close() {
    if (pwm_path_.empty()) return;
    std::string ignored_error;
    SetEnabled(false, ignored_error);
    if (exported_by_us_) {
        linux_platform::WriteAttribute(chip_path_ + "/unexport", "0", ignored_error);
    }
    pwm_path_.clear();
    chip_path_.clear();
    exported_by_us_ = false;
    enabled_ = false;
}

}  // namespace clockin
