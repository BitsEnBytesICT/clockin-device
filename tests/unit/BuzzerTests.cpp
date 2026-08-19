#include "TestHarness.hpp"

#include "services/hardware/buzzer/BuzzerSequencer.hpp"

#include <vector>

namespace {

class FakeBuzzerOutput final : public clockin::IBuzzerOutput {
public:
    bool SetEnabled(bool enabled, std::string&) override {
        states.push_back(enabled);
        return succeeds;
    }

    bool succeeds = true;
    std::vector<bool> states;
};

}  // namespace

TEST_CASE("single beep always ends with output disabled") {
    FakeBuzzerOutput output;
    clockin::BuzzerSequencer buzzer(output);
    std::string error;
    CHECK(buzzer.Start(clockin::BuzzerPattern::Single, 10.0, error));
    CHECK(buzzer.Active());
    CHECK(buzzer.Update(10.051, error));
    CHECK(!buzzer.Active());
    CHECK(output.states == std::vector<bool>({true, false}));
}

TEST_CASE("triple buzz has three nonblocking pulses") {
    FakeBuzzerOutput output;
    clockin::BuzzerSequencer buzzer(output);
    std::string error;
    CHECK(buzzer.Start(clockin::BuzzerPattern::Triple, 0.0, error));
    for (double now : {0.051, 0.101, 0.151, 0.201, 0.251}) {
        CHECK(buzzer.Update(now, error));
    }
    CHECK(output.states == std::vector<bool>({true, false, true, false, true, false}));
    CHECK(!buzzer.Active());
}
