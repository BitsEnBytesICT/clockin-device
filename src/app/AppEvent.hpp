#pragma once

#include "imgui.h"
#include "ui/components/UiControl.hpp"

namespace clockin {

struct AppEvent {
    UiControl control = UiControl::None;
    double occurred_at = 0.0;
    ImVec2 position{0.0f, 0.0f};
};

}  // namespace clockin
