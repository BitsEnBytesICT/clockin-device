#pragma once

#include "services/hardware/IHardwareService.hpp"

#include <memory>

namespace clockin {

class HardwareService final : public IHardwareService {
public:
    HardwareService();
    ~HardwareService() override;

    HardwareService(const HardwareService&) = delete;
    HardwareService& operator=(const HardwareService&) = delete;

    void Update(double now_seconds) override;
    void Stop() override;
    bool PopCard(RfidCard& card) override;
    void PlayFeedback(FeedbackCue cue) override;
    void CancelFeedback() override;
    bool RfidOpen() const override;
    unsigned int ReconnectCount() const override;
    std::size_t PendingFeedbackCount() const override;
    std::string LastError() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace clockin
