#pragma once

#include "imgui.h"

namespace clockin {

struct PageTransitionState {
    float opacity = 1.0f;
    float offset_y = 0.0f;
};

PageTransitionState EvaluatePageTransition(double elapsed_seconds);
void ApplyPageTransition(ImDrawList& draw,
                         int first_vertex,
                         const PageTransitionState& state);

}  // namespace clockin
