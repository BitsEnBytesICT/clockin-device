#include "domain/CardPresenceTracker.hpp"

namespace clockin {

CardPresenceTracker::CardPresenceTracker(double removal_timeout_seconds)
    : removal_timeout_(removal_timeout_seconds) {}

bool CardPresenceTracker::Observe(const std::string& uid, double now_seconds) {
    const bool new_presentation = !present_ || uid != uid_;
    present_ = true;
    uid_ = uid;
    last_seen_ = now_seconds;
    return new_presentation;
}

bool CardPresenceTracker::Update(double now_seconds) {
    if (!present_ || now_seconds - last_seen_ < removal_timeout_) return false;
    ForceAbsent();
    return true;
}

void CardPresenceTracker::ForceAbsent() {
    present_ = false;
    uid_.clear();
    last_seen_ = 0.0;
}

}  // namespace clockin
