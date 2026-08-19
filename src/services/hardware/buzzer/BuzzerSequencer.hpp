#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace clockin {

class IBuzzerOutput {
public:
    virtual ~IBuzzerOutput() = default;
    virtual bool SetEnabled(bool enabled, std::string& error) = 0;
};

enum class BuzzerPattern { Single, Triple };

class BuzzerSequencer {
public:
    explicit BuzzerSequencer(IBuzzerOutput& output);

    bool Start(BuzzerPattern pattern, double now_seconds, std::string& error);
    bool Update(double now_seconds, std::string& error);
    void Cancel();
    bool Active() const { return active_; }

private:
    IBuzzerOutput& output_;
    std::vector<double> phases_;
    std::size_t phase_ = 0;
    double deadline_ = 0.0;
    bool active_ = false;
};

}  // namespace clockin
