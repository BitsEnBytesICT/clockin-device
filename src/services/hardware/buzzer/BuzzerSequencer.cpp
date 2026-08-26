#include "services/hardware/buzzer/BuzzerSequencer.hpp"

namespace clockin {
namespace {

constexpr BuzzerTone Tone(std::uint32_t frequency_hz,
                          std::uint8_t duty_percent) {
    return {frequency_hz, duty_percent};
}

constexpr BuzzerTone Silence() { return {}; }

}  // namespace

BuzzerSequencer::BuzzerSequencer(IBuzzerOutput& output) : output_(output) {}

const std::vector<BuzzerSequencer::Phase>& BuzzerSequencer::PhasesFor(
    BuzzerPattern pattern) {
    // Short, restrained cues centered around the buzzer's proven 2 kHz range.
    static const std::vector<Phase> tap{{Tone(2300, 10), 0.015}};
    static const std::vector<Phase> card{{Tone(2100, 13), 0.026}};
    static const std::vector<Phase> success{
        {Tone(1750, 15), 0.032},
        {Silence(), 0.014},
        {Tone(2300, 18), 0.065},
    };
    static const std::vector<Phase> warning{
        {Tone(1500, 13), 0.035},
        {Silence(), 0.040},
        {Tone(1500, 13), 0.045},
    };
    static const std::vector<Phase> error{
        {Tone(1700, 15), 0.055},
        {Silence(), 0.025},
        {Tone(1250, 15), 0.085},
    };
    switch (pattern) {
        case BuzzerPattern::Tap: return tap;
        case BuzzerPattern::CardAccepted: return card;
        case BuzzerPattern::Success: return success;
        case BuzzerPattern::Warning: return warning;
        case BuzzerPattern::Error: return error;
    }
    return tap;
}

bool BuzzerSequencer::ApplyPhase(const Phase& phase, std::string& error) {
    return phase.tone.frequency_hz == 0 ? output_.Stop(error)
                                        : output_.PlayTone(phase.tone, error);
}

bool BuzzerSequencer::Start(BuzzerPattern pattern,
                            double now_seconds,
                            std::string& error) {
    Cancel();
    phases_ = &PhasesFor(pattern);
    if (phases_->empty() || !ApplyPhase(phases_->front(), error)) {
        phases_ = nullptr;
        return false;
    }
    phase_ = 0;
    deadline_ = now_seconds + phases_->front().duration_seconds;
    active_ = true;
    return true;
}

bool BuzzerSequencer::Update(double now_seconds, std::string& error) {
    while (active_ && now_seconds >= deadline_) {
        ++phase_;
        if (phase_ >= phases_->size()) {
            const bool stopped = output_.Stop(error);
            active_ = false;
            phases_ = nullptr;
            return stopped;
        }
        if (!ApplyPhase((*phases_)[phase_], error)) {
            std::string ignored_error;
            output_.Stop(ignored_error);
            active_ = false;
            phases_ = nullptr;
            return false;
        }
        deadline_ += (*phases_)[phase_].duration_seconds;
    }
    return true;
}

void BuzzerSequencer::Cancel() {
    if (active_) {
        std::string ignored_error;
        output_.Stop(ignored_error);
    }
    phases_ = nullptr;
    phase_ = 0;
    active_ = false;
}

}  // namespace clockin
