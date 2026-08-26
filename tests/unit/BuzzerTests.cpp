#include "TestHarness.hpp"

#include "services/hardware/buzzer/BuzzerSequencer.hpp"

#include <vector>

namespace {

struct OutputEvent {
    bool playing = false;
    clockin::BuzzerTone tone;
};

class FakeBuzzerOutput final : public clockin::IBuzzerOutput {
public:
    bool PlayTone(const clockin::BuzzerTone& tone, std::string&) override {
        events.push_back({true, tone});
        return true;
    }

    bool Stop(std::string&) override {
        events.push_back({false, {}});
        return true;
    }

    std::vector<OutputEvent> events;
};

void CheckTone(const OutputEvent& event,
               std::uint32_t frequency_hz,
               std::uint8_t duty_percent) {
    CHECK(event.playing);
    CHECK(event.tone.frequency_hz == frequency_hz);
    CHECK(event.tone.duty_percent == duty_percent);
}

}  // namespace

TEST_CASE("subtle tap and card cues finish with the buzzer disabled") {
    FakeBuzzerOutput output;
    clockin::BuzzerSequencer buzzer(output);
    std::string error;

    CHECK(buzzer.Start(clockin::BuzzerPattern::Tap, 10.0, error));
    CheckTone(output.events.front(), 2300, 10);
    CHECK(buzzer.Update(10.016, error));
    CHECK(!buzzer.Active());
    CHECK(!output.events.back().playing);

    output.events.clear();
    CHECK(buzzer.Start(clockin::BuzzerPattern::CardAccepted, 20.0, error));
    CheckTone(output.events.front(), 2100, 13);
    CHECK(buzzer.Update(20.027, error));
    CHECK(!buzzer.Active());
    CHECK(!output.events.back().playing);
}

TEST_CASE("success cue is short restrained and rises in pitch") {
    FakeBuzzerOutput output;
    clockin::BuzzerSequencer buzzer(output);
    std::string error;

    CHECK(buzzer.Start(clockin::BuzzerPattern::Success, 0.0, error));
    CHECK(buzzer.Update(0.033, error));
    CHECK(buzzer.Update(0.047, error));
    CHECK(buzzer.Update(0.112, error));

    CHECK(output.events.size() == 4);
    CheckTone(output.events[0], 1750, 15);
    CHECK(!output.events[1].playing);
    CheckTone(output.events[2], 2300, 18);
    CHECK(!output.events[3].playing);
    CHECK(!buzzer.Active());
}

TEST_CASE("warning and error cues stay bounded and errors descend in pitch") {
    FakeBuzzerOutput output;
    clockin::BuzzerSequencer buzzer(output);
    std::string error;

    CHECK(buzzer.Start(clockin::BuzzerPattern::Warning, 0.0, error));
    CHECK(buzzer.Update(0.036, error));
    CHECK(buzzer.Update(0.076, error));
    CHECK(buzzer.Update(0.121, error));
    CHECK(output.events.size() == 4);
    CheckTone(output.events[0], 1500, 13);
    CheckTone(output.events[2], 1500, 13);

    output.events.clear();
    CHECK(buzzer.Start(clockin::BuzzerPattern::Error, 1.0, error));
    CHECK(buzzer.Update(1.056, error));
    CHECK(buzzer.Update(1.081, error));
    CHECK(buzzer.Update(1.166, error));
    CHECK(output.events.size() == 4);
    CheckTone(output.events[0], 1700, 15);
    CheckTone(output.events[2], 1250, 15);
    CHECK(!output.events.back().playing);
    CHECK(!buzzer.Active());
}
