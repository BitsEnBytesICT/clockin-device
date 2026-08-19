#include "services/hardware/buzzer/BuzzerSequencer.hpp"

namespace clockin {

BuzzerSequencer::BuzzerSequencer(IBuzzerOutput& output) : output_(output) {}

bool BuzzerSequencer::Start(BuzzerPattern pattern,
                            double now_seconds,
                            std::string& error) {
    Cancel();
    phases_ = pattern == BuzzerPattern::Triple
                  ? std::vector<double>{0.05, 0.05, 0.05, 0.05, 0.05}
                  : std::vector<double>{0.05};
    if (!output_.SetEnabled(true, error)) {
        phases_.clear();
        return false;
    }
    phase_ = 0;
    deadline_ = now_seconds + phases_.front();
    active_ = true;
    return true;
}

bool BuzzerSequencer::Update(double now_seconds, std::string& error) {
    while (active_ && now_seconds >= deadline_) {
        ++phase_;
        if (phase_ >= phases_.size()) {
            if (!output_.SetEnabled(false, error)) return false;
            active_ = false;
            phases_.clear();
            return true;
        }
        const bool enabled = phase_ % 2 == 0;
        if (!output_.SetEnabled(enabled, error)) {
            std::string ignored_error;
            output_.SetEnabled(false, ignored_error);
            active_ = false;
            phases_.clear();
            return false;
        }
        deadline_ += phases_[phase_];
    }
    return true;
}

void BuzzerSequencer::Cancel() {
    if (active_) {
        std::string ignored_error;
        output_.SetEnabled(false, ignored_error);
    }
    phases_.clear();
    phase_ = 0;
    active_ = false;
}

}  // namespace clockin
