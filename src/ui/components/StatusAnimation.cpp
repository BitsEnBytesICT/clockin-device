#include "ui/components/StatusAnimation.hpp"

#include "ui/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace clockin {
namespace {

constexpr float kSuccessCircleRadius = 60.0f;
constexpr float kErrorCircleRadius = 58.0f;
constexpr float kCheckThickness = 7.0f;
constexpr float kErrorThickness = 6.0f;

float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float SmoothStep(float value) {
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float EaseOutCubic(float value) {
    const float t = Clamp01(value);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

float EaseInOutCubic(float value) {
    const float t = Clamp01(value);
    if (t < 0.5f) return 4.0f * t * t * t;
    const float inverse = -2.0f * t + 2.0f;
    return 1.0f - inverse * inverse * inverse * 0.5f;
}

float EaseOutBack(float value) {
    constexpr float kOvershoot = 1.70158f;
    constexpr float kCurve = kOvershoot + 1.0f;
    const float shifted = Clamp01(value) - 1.0f;
    return 1.0f + kCurve * shifted * shifted * shifted +
           kOvershoot * shifted * shifted;
}

ImVec2 Interpolate(const ImVec2& start, const ImVec2& end, float progress) {
    return {start.x + (end.x - start.x) * progress,
            start.y + (end.y - start.y) * progress};
}

float Distance(const ImVec2& start, const ImVec2& end) {
    return std::hypot(end.x - start.x, end.y - start.y);
}

ImU32 WithOpacity(ImU32 color, float opacity) {
    const int source_alpha = static_cast<int>((color >> 24) & 0xFFu);
    const int alpha = static_cast<int>(source_alpha * Clamp01(opacity) + 0.5f);
    return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
}

void DrawRoundedLine(ImDrawList& draw,
                     const ImVec2& start,
                     const ImVec2& end,
                     float progress,
                     ImU32 color,
                     float thickness) {
    if (progress <= 0.0f) return;
    const ImVec2 current = Interpolate(start, end, Clamp01(progress));
    const float radius = thickness * 0.5f;
    draw.AddLine(start, current, color, thickness);
    draw.AddCircleFilled(start, radius, color, 16);
    draw.AddCircleFilled(current, radius, color, 16);
}

}  // namespace

SuccessAnimationState EvaluateSuccessAnimation(double elapsed_seconds) {
    const float elapsed = static_cast<float>(std::max(0.0, elapsed_seconds));
    const float circle_progress = Clamp01(elapsed / 0.28f);
    const float check_progress = Clamp01((elapsed - 0.10f) / 0.55f);
    const float halo_progress = Clamp01((elapsed - 0.14f) / 0.56f);

    SuccessAnimationState state;
    state.circle_scale = 0.82f + 0.18f * EaseOutBack(circle_progress);
    state.circle_opacity = SmoothStep(elapsed / 0.18f);
    state.check_progress = EaseInOutCubic(check_progress);
    state.halo_progress = EaseOutCubic(halo_progress);
    state.halo_opacity = 1.12f * halo_progress * (1.0f - halo_progress);
    return state;
}

ErrorAnimationState EvaluateErrorAnimation(double elapsed_seconds) {
    const float elapsed = static_cast<float>(std::max(0.0, elapsed_seconds));
    const float circle_progress = Clamp01(elapsed / 0.26f);
    const float first_stroke = Clamp01((elapsed - 0.10f) / 0.34f);
    const float second_stroke = Clamp01((elapsed - 0.22f) / 0.34f);
    const float halo_progress = Clamp01((elapsed - 0.12f) / 0.52f);

    ErrorAnimationState state;
    state.circle_scale = 0.82f + 0.18f * EaseOutBack(circle_progress);
    state.circle_opacity = SmoothStep(elapsed / 0.17f);
    state.first_stroke_progress = EaseInOutCubic(first_stroke);
    state.second_stroke_progress = EaseInOutCubic(second_stroke);
    state.halo_progress = EaseOutCubic(halo_progress);
    state.halo_opacity = 0.96f * halo_progress * (1.0f - halo_progress);
    return state;
}

SuccessCheckmarkGeometry BuildSuccessCheckmarkGeometry(const ImVec2& center,
                                                       float scale,
                                                       float progress) {
    const ImVec2 start{center.x - 24.0f * scale, center.y};
    const ImVec2 joint{center.x - 6.0f * scale, center.y + 18.0f * scale};
    const ImVec2 end{center.x + 31.0f * scale, center.y - 25.0f * scale};
    const float first_length = Distance(start, joint);
    const float second_length = Distance(joint, end);
    const float draw_length = Clamp01(progress) * (first_length + second_length);

    SuccessCheckmarkGeometry geometry;
    if (draw_length <= 0.0f) return geometry;

    geometry.points[0] = start;
    if (draw_length < first_length) {
        geometry.points[1] = Interpolate(start, joint, draw_length / first_length);
        geometry.point_count = 2;
        return geometry;
    }

    geometry.points[1] = joint;
    geometry.points[2] = Interpolate(
        joint, end, std::min(1.0f, (draw_length - first_length) / second_length));
    geometry.point_count = 3;
    return geometry;
}

void DrawSuccessAnimation(ImDrawList& draw,
                          const ImVec2& center,
                          double elapsed_seconds) {
    const SuccessAnimationState state = EvaluateSuccessAnimation(elapsed_seconds);
    if (state.halo_opacity > 0.0f) {
        const float halo_radius = kSuccessCircleRadius + 18.0f * state.halo_progress;
        draw.AddCircle(center, halo_radius,
                       WithOpacity(theme::GreenText, state.halo_opacity),
                       48, 2.0f);
    }

    draw.AddCircleFilled(center, kSuccessCircleRadius * state.circle_scale,
                         WithOpacity(theme::Green, state.circle_opacity), 48);

    const SuccessCheckmarkGeometry geometry = BuildSuccessCheckmarkGeometry(
        center, state.circle_scale, state.check_progress);
    if (geometry.point_count < 2) return;

    const float thickness = kCheckThickness * state.circle_scale;
    const float cap_radius = thickness * 0.5f;
    const ImU32 check_color = WithOpacity(theme::GreenText, state.circle_opacity);
    draw.AddPolyline(geometry.points.data(), geometry.point_count, check_color,
                     ImDrawFlags_None, thickness);
    draw.AddCircleFilled(geometry.points.front(), cap_radius, check_color, 16);
    if (geometry.point_count == 3) {
        draw.AddCircleFilled(geometry.points[1], cap_radius, check_color, 16);
    }
    draw.AddCircleFilled(geometry.points[geometry.point_count - 1], cap_radius,
                         check_color, 16);
}

void DrawErrorAnimation(ImDrawList& draw,
                        const ImVec2& center,
                        double elapsed_seconds) {
    const ErrorAnimationState state = EvaluateErrorAnimation(elapsed_seconds);
    if (state.halo_opacity > 0.0f) {
        const float halo_radius = kErrorCircleRadius + 16.0f * state.halo_progress;
        draw.AddCircle(center, halo_radius,
                       WithOpacity(theme::RedText, state.halo_opacity),
                       48, 2.0f);
    }

    draw.AddCircleFilled(center, kErrorCircleRadius * state.circle_scale,
                         WithOpacity(theme::Red, state.circle_opacity), 48);

    const float inset = 20.0f * state.circle_scale;
    const float thickness = kErrorThickness * state.circle_scale;
    const ImU32 stroke_color = WithOpacity(theme::White, state.circle_opacity);
    DrawRoundedLine(draw, {center.x - inset, center.y - inset},
                    {center.x + inset, center.y + inset},
                    state.first_stroke_progress, stroke_color, thickness);
    DrawRoundedLine(draw, {center.x + inset, center.y - inset},
                    {center.x - inset, center.y + inset},
                    state.second_stroke_progress, stroke_color, thickness);
}

}  // namespace clockin
