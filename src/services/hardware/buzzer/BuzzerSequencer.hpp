#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace clockin {

struct BuzzerTone {
    std::uint32_t frequency_hz = 0;
    std::uint8_t duty_percent = 0;
};

class IBuzzerOutput {
public:
    virtual ~IBuzzerOutput() = default;
    virtual bool PlayTone(const BuzzerTone& tone, std::string& error) = 0;
    virtual bool Stop(std::string& error) = 0;
};

enum class BuzzerPattern { Tap, CardAccepted, Success, Warning, Error };

class BuzzerSequencer {
public:
    explicit BuzzerSequencer(IBuzzerOutput& output);

    bool Start(BuzzerPattern pattern, double now_seconds, std::string& error);
    bool Update(double now_seconds, std::string& error);
    void Cancel();
    bool Active() const { return active_; }

private:
    struct Phase {
        BuzzerTone tone;
        double duration_seconds = 0.0;
    };

    IBuzzerOutput& output_;
    const std::vector<Phase>* phases_ = nullptr;
    std::size_t phase_ = 0;
    double deadline_ = 0.0;
    bool active_ = false;

    static const std::vector<Phase>& PhasesFor(BuzzerPattern pattern);
    bool ApplyPhase(const Phase& phase, std::string& error);
};

}  // namespace clockin
