#include "platform/linux/LinuxPwmBuzzerOutput.hpp"

#include "platform/linux/LinuxPwm.hpp"

#include <cstdint>

namespace clockin {

bool LinuxPwmBuzzerOutput::PlayTone(const BuzzerTone& tone,
                                    std::string& error) {
    if (tone.frequency_hz < 100 || tone.frequency_hz > 20000 ||
        tone.duty_percent == 0 || tone.duty_percent >= 50) {
        error = "Invalid buzzer tone";
        return false;
    }
    constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;
    const std::uint64_t period =
        (kNanosecondsPerSecond + tone.frequency_hz / 2U) / tone.frequency_hz;
    const std::uint64_t duty = period * tone.duty_percent / 100U;
    if ((!pwm_.IsOpen() && !pwm_.OpenTim16Channel1(error)) ||
        !pwm_.Configure(period, duty, error)) {
        return false;
    }
    return pwm_.SetEnabled(true, error);
}

bool LinuxPwmBuzzerOutput::Stop(std::string& error) {
    return pwm_.SetEnabled(false, error);
}

}  // namespace clockin
