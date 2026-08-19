#pragma once

#include <cstdint>
#include <string>

namespace clockin {

class LinuxPwm {
public:
    ~LinuxPwm();
    bool OpenTim16Channel1(std::string& error);
    bool Configure(std::uint64_t period_nanoseconds,
                   std::uint64_t duty_nanoseconds,
                   std::string& error);
    bool SetEnabled(bool enabled, std::string& error);
    void Close();
    bool IsOpen() const { return !pwm_path_.empty(); }
    const std::string& PwmPath() const { return pwm_path_; }

private:
    std::string chip_path_;
    std::string pwm_path_;
    bool exported_by_us_ = false;
    bool enabled_ = false;
};

}  // namespace clockin
