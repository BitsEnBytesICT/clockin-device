#pragma once

#include "services/hardware/IHardwareService.hpp"

#include <deque>
#include <utility>

namespace clockin::test {

class FakeHardwareService final : public IHardwareService {
public:
    void Update(double) override { ++updates; }
    void Stop() override { stopped = true; }
    bool PopCard(RfidCard& card) override {
        if (cards.empty()) return false;
        card = std::move(cards.front());
        cards.pop_front();
        return true;
    }
    void Beep() override { ++beeps; }
    void Buzz() override { ++buzzes; }
    void CancelFeedback() override { ++cancellations; }
    bool RfidOpen() const override { return rfid_open; }
    unsigned int ReconnectCount() const override { return reconnects; }
    std::size_t PendingFeedbackCount() const override { return 0; }
    std::string LastError() const override { return error; }

    void Present(std::string uid) { cards.push_back({std::move(uid), true}); }

    std::deque<RfidCard> cards;
    int updates = 0;
    int beeps = 0;
    int buzzes = 0;
    int cancellations = 0;
    bool stopped = false;
    bool rfid_open = true;
    unsigned int reconnects = 0;
    std::string error;
};

}  // namespace clockin::test
