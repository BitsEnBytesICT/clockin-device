#pragma once

#include "imgui.h"
#include "ui/components/UiControl.hpp"

#include <cstddef>
#include <string>

namespace clockin {

class PageDrawing;

struct PinKeypadBounds {
    ImVec2 minimum;
    ImVec2 maximum;
};

class PinKeypad {
public:
    static UiControl HitTest(const ImVec2& point, const PinKeypadBounds& bounds);
    static int Digit(UiControl control);
    static bool Apply(UiControl control, std::string& value, std::size_t maximum = 4);
    static void Render(ImDrawList& draw,
                       PageDrawing& drawing,
                       const PinKeypadBounds& bounds,
                       UiControl last_key,
                       double last_key_time);
};

}  // namespace clockin
