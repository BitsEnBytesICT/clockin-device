#pragma once

#include "services/hardware/IHardwareService.hpp"

#include <algorithm>
#include <deque>
#include <vector>
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
    void PlayFeedback(FeedbackCue cue) override { feedback.push_back(cue); }
    void CancelFeedback() override { ++cancellations; }
    bool RfidOpen() const override { return rfid_open; }
    unsigned int ReconnectCount() const override { return reconnects; }
    std::size_t PendingFeedbackCount() const override { return 0; }
    std::string LastError() const override { return error; }

    void Present(std::string uid) { cards.push_back({std::move(uid), true}); }
    int FeedbackCount(FeedbackCue cue) const {
        return static_cast<int>(std::count(feedback.begin(), feedback.end(), cue));
    }

    std::deque<RfidCard> cards;
    int updates = 0;
    std::vector<FeedbackCue> feedback;
    int cancellations = 0;
    bool stopped = false;
    bool rfid_open = true;
    unsigned int reconnects = 0;
    std::string error;
};

}  // namespace clockin::test
