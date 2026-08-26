#pragma once

#include <cstddef>
#include <string>

namespace clockin {

struct RfidCard {
    std::string uid;
    bool valid = false;
};

enum class FeedbackCue {
    Tap,
    CardAccepted,
    Success,
    Warning,
    Error
};

class IHardwareService {
public:
    virtual ~IHardwareService() = default;
    virtual void Update(double now_seconds) = 0;
    virtual void Stop() = 0;
    virtual bool PopCard(RfidCard& card) = 0;
    virtual void PlayFeedback(FeedbackCue cue) = 0;
    virtual void CancelFeedback() = 0;
    virtual bool RfidOpen() const = 0;
    virtual unsigned int ReconnectCount() const = 0;
    virtual std::size_t PendingFeedbackCount() const = 0;
    virtual std::string LastError() const = 0;
};

}  // namespace clockin
