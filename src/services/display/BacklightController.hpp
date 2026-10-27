#pragma once

#include <string>

namespace clockin {

class BacklightController {
public:
    BacklightController(std::string brightness_path,
                        std::string maximum_path,
                        int full_value = 240,
                        int dim_value = 120);

    void Initialize();
    void Update(bool activity_detected, double delta, double now_seconds);
    int Maximum() const { return maximum_; }
    // True only once the panel has actually been written to zero brightness.
    bool IsOff() const { return target_ == 0 && current_ == 0; }

private:
    std::string brightness_path_;
    std::string maximum_path_;
    int full_value_;
    int dim_value_;
    int maximum_ = 240;
    int current_ = -1;
    double current_float_ = -1.0;
    int target_ = -1;
    double transition_start_ = 0.0;
    double transition_progress_ = 1.0;
    double last_write_ = -1.0;
    double inactivity_seconds_ = 0.0;

    int ReadMaximum() const;
    bool Write(int value) const;
    void SetTarget(int target);
};

}  // namespace clockin
