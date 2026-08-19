#include "services/display/BacklightController.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace clockin {
namespace {

constexpr double kDimSeconds = 30.0;
constexpr double kOffSeconds = 60.0;
constexpr double kTransitionSeconds = 0.5;

}  // namespace

BacklightController::BacklightController(std::string brightness_path,
                                         std::string maximum_path,
                                         int full_value,
                                         int dim_value)
    : brightness_path_(std::move(brightness_path)),
      maximum_path_(std::move(maximum_path)),
      full_value_(full_value),
      dim_value_(dim_value) {}

void BacklightController::Initialize() {
    maximum_ = ReadMaximum();
    current_float_ = std::min(full_value_, maximum_);
    target_ = static_cast<int>(current_float_);
    Write(target_);
    current_ = target_;
}

void BacklightController::Update(bool activity_detected, double delta, double now_seconds) {
    if (activity_detected) {
        inactivity_seconds_ = 0.0;
        SetTarget(std::min(full_value_, maximum_));
    } else {
        inactivity_seconds_ += delta;
        if (inactivity_seconds_ >= kOffSeconds) SetTarget(0);
        else if (inactivity_seconds_ >= kDimSeconds) SetTarget(std::min(dim_value_, maximum_));
    }
    if (target_ < 0) return;
    transition_progress_ =
        std::min(1.0, transition_progress_ + delta / kTransitionSeconds);
    current_float_ = transition_start_ +
                     (target_ - transition_start_) * transition_progress_;
    const int value = static_cast<int>(current_float_ + 0.5);
    if (value != current_ && now_seconds - last_write_ >= 0.05) {
        if (Write(value)) current_ = value;
        last_write_ = now_seconds;
    }
}

int BacklightController::ReadMaximum() const {
    const int descriptor = open(maximum_path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return full_value_;
    char value[16] = {};
    const ssize_t count = read(descriptor, value, sizeof(value) - 1);
    close(descriptor);
    const int parsed = count > 0 ? std::atoi(value) : 0;
    return parsed > 0 ? parsed : full_value_;
}

bool BacklightController::Write(int value) const {
    const int descriptor = open(brightness_path_.c_str(), O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) return false;
    char buffer[16];
    const int length = std::snprintf(buffer, sizeof(buffer), "%d", value);
    ssize_t offset = 0;
    while (offset < length) {
        const ssize_t written =
            write(descriptor, buffer + offset, static_cast<std::size_t>(length - offset));
        if (written > 0) offset += written;
        else if (written < 0 && errno == EINTR) continue;
        else break;
    }
    close(descriptor);
    return offset == length;
}

void BacklightController::SetTarget(int target) {
    target = std::max(0, std::min(maximum_, target));
    if (target == target_) return;
    transition_start_ = current_float_ < 0.0 ? target : current_float_;
    target_ = target;
    transition_progress_ = 0.0;
}

}  // namespace clockin
