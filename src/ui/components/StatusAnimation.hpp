#pragma once

#include "imgui.h"

#include <array>

namespace clockin {

struct SuccessAnimationState {
    float circle_scale = 0.82f;
    float circle_opacity = 0.0f;
    float check_progress = 0.0f;
    float halo_progress = 0.0f;
    float halo_opacity = 0.0f;
};

struct ErrorAnimationState {
    float circle_scale = 0.82f;
    float circle_opacity = 0.0f;
    float first_stroke_progress = 0.0f;
    float second_stroke_progress = 0.0f;
    float halo_progress = 0.0f;
    float halo_opacity = 0.0f;
};

struct SuccessCheckmarkGeometry {
    std::array<ImVec2, 3> points{};
    int point_count = 0;
};

SuccessAnimationState EvaluateSuccessAnimation(double elapsed_seconds);
ErrorAnimationState EvaluateErrorAnimation(double elapsed_seconds);
SuccessCheckmarkGeometry BuildSuccessCheckmarkGeometry(const ImVec2& center,
                                                       float scale,
                                                       float progress);
void DrawSuccessAnimation(ImDrawList& draw,
                          const ImVec2& center,
                          double elapsed_seconds);
void DrawErrorAnimation(ImDrawList& draw,
                        const ImVec2& center,
                        double elapsed_seconds);

}  // namespace clockin
