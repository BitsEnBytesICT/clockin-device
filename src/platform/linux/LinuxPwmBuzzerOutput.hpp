#pragma once

#include "services/hardware/buzzer/BuzzerSequencer.hpp"

namespace clockin {

class LinuxPwm;

class LinuxPwmBuzzerOutput final : public IBuzzerOutput {
public:
    explicit LinuxPwmBuzzerOutput(LinuxPwm& pwm) : pwm_(pwm) {}

    bool PlayTone(const BuzzerTone& tone, std::string& error) override;
    bool Stop(std::string& error) override;

private:
    LinuxPwm& pwm_;
};

}  // namespace clockin
