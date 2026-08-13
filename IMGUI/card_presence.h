#ifndef CARD_PRESENCE_H
#define CARD_PRESENCE_H

#include <string>

class CardPresenceTracker {
public:
    explicit CardPresenceTracker(double removal_timeout_seconds = 1.5)
        : removal_timeout_(removal_timeout_seconds), present_(false), last_seen_(0.0) {}

    bool Observe(const std::string& uid, double now_seconds) {
        const bool new_presentation = !present_ || uid != uid_;
        present_ = true;
        uid_ = uid;
        last_seen_ = now_seconds;
        return new_presentation;
    }

    bool Update(double now_seconds) {
        if (present_ && now_seconds - last_seen_ >= removal_timeout_) {
            ForceAbsent();
            return true;
        }
        return false;
    }

    void ForceAbsent() {
        present_ = false;
        uid_.clear();
        last_seen_ = 0.0;
    }

    bool IsPresent() const { return present_; }
    const std::string& UID() const { return uid_; }
    double LastSeen() const { return last_seen_; }

private:
    double removal_timeout_;
    bool present_;
    std::string uid_;
    double last_seen_;
};

#endif
