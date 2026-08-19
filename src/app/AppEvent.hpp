#pragma once

#include "ui/components/UiControl.hpp"

namespace clockin {

struct AppEvent {
    UiControl control = UiControl::None;
    double occurred_at = 0.0;
};

}  // namespace clockin
