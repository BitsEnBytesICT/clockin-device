#pragma once

#include <string>

namespace clockin {

class CardPresenceTracker {
public:
    explicit CardPresenceTracker(double removal_timeout_seconds = 1.5);

    bool Observe(const std::string& uid, double now_seconds);
    bool Update(double now_seconds);
    void ForceAbsent();

    bool IsPresent() const { return present_; }
    const std::string& Uid() const { return uid_; }
    double LastSeen() const { return last_seen_; }

private:
    double removal_timeout_;
    bool present_ = false;
    std::string uid_;
    double last_seen_ = 0.0;
};

}  // namespace clockin
