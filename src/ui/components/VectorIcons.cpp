#include "ui/components/VectorIcons.hpp"

#include <algorithm>
#include <cmath>

namespace clockin {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float Clamp01(float value) { return std::max(0.0f, std::min(1.0f, value)); }

ImVec2 CirclePoint(const ImVec2& center, float radius, float angle) {
    return {center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius};
}

void DrawArcArrow(ImDrawList& draw,
                  const ImVec2& center,
                  float radius,
                  float start,
                  float end,
                  ImU32 color,
                  float stroke,
                  float size) {
    draw.PathArcTo(center, radius, start, end, 18);
    draw.PathStroke(color, 0, stroke);
    const ImVec2 tip = CirclePoint(center, radius, end);
    const ImVec2 tangent{-std::sin(end), std::cos(end)};
    const ImVec2 radial{std::cos(end), std::sin(end)};
    const ImVec2 base{tip.x - tangent.x * size * 0.22f,
                      tip.y - tangent.y * size * 0.22f};
    draw.AddTriangleFilled(tip,
                           {base.x + radial.x * size * 0.10f,
                            base.y + radial.y * size * 0.10f},
                           {base.x - radial.x * size * 0.10f,
                            base.y - radial.y * size * 0.10f},
                           color);
}

}  // namespace

void DrawVectorIcon(ImDrawList& draw,
                    VectorIcon icon,
                    const ImVec2& center,
                    float size,
                    ImU32 color,
                    float progress) {
    const float half = size * 0.5f;
    const float stroke = std::max(1.5f, size * 0.085f);
    progress = Clamp01(progress);
    if (icon == VectorIcon::None) {
        return;
    } else if (icon == VectorIcon::Card) {
        draw.AddRect({center.x - half, center.y - half * 0.72f},
                     {center.x + half, center.y + half * 0.72f}, color,
                     size * 0.14f, 0, stroke);
        draw.AddLine({center.x - half * 0.68f, center.y - half * 0.20f},
                     {center.x + half * 0.30f, center.y - half * 0.20f}, color, stroke);
        draw.AddLine({center.x - half * 0.68f, center.y + half * 0.24f},
                     {center.x + half * 0.58f, center.y + half * 0.24f}, color, stroke);
    } else if (icon == VectorIcon::Pin) {
        for (int index = 0; index < 4; ++index) {
            const float x = center.x - size * 0.30f + index * size * 0.20f;
            draw.AddCircleFilled({x, center.y}, size * 0.075f, color, 14);
        }
    } else if (icon == VectorIcon::Wifi) {
        const ImVec2 origin{center.x, center.y + size * 0.35f};
        for (int ring = 3; ring >= 1; --ring) {
            const float radius = size * (0.13f + ring * 0.135f);
            draw.PathArcTo(origin, radius, kPi * 1.22f, kPi * 1.78f, 24);
            draw.PathStroke(color, 0, stroke);
        }
        draw.AddCircleFilled(origin, size * 0.075f,
                             color, 14);
    } else if (icon == VectorIcon::Lock) {
        draw.AddRect({center.x - half * 0.62f, center.y - half * 0.02f},
                     {center.x + half * 0.62f, center.y + half * 0.78f}, color,
                     size * 0.09f, 0, stroke);
        draw.PathArcTo({center.x, center.y - half * 0.02f}, half * 0.42f,
                       kPi, kPi * 2.0f, 18);
        draw.PathStroke(color, 0, stroke);
    } else if (icon == VectorIcon::Globe) {
        draw.AddCircle(center, half * 0.88f, color, 28, stroke);
        draw.AddEllipse(center, {half * 0.42f, half * 0.88f}, color, 0.0f, 24, stroke);
        draw.AddLine({center.x - half * 0.83f, center.y},
                     {center.x + half * 0.83f, center.y}, color, stroke);
    } else if (icon == VectorIcon::Refresh) {
        const float rotation = progress * 2.0f * kPi;
        DrawArcArrow(draw, center, half * 0.72f, rotation - 0.15f,
                     rotation + kPi * 0.72f, color, stroke, size);
        DrawArcArrow(draw, center, half * 0.72f, rotation + kPi - 0.15f,
                     rotation + kPi * 1.72f, color, stroke, size);
    } else if (icon == VectorIcon::Check) {
        const ImVec2 first{center.x - half * 0.70f, center.y};
        const ImVec2 joint{center.x - half * 0.18f, center.y + half * 0.48f};
        const ImVec2 end{center.x + half * 0.78f, center.y - half * 0.56f};
        if (progress <= 0.42f) {
            const float part = progress / 0.42f;
            draw.AddLine(first, {first.x + (joint.x - first.x) * part,
                                 first.y + (joint.y - first.y) * part}, color, stroke);
        } else {
            draw.AddLine(first, joint, color, stroke);
            const float part = (progress - 0.42f) / 0.58f;
            draw.AddLine(joint, {joint.x + (end.x - joint.x) * part,
                                 joint.y + (end.y - joint.y) * part}, color, stroke);
        }
    } else if (icon == VectorIcon::Eye || icon == VectorIcon::EyeOff) {
        draw.PathLineTo({center.x - half, center.y});
        draw.PathBezierCubicCurveTo({center.x - half * 0.48f, center.y - half * 0.70f},
                                    {center.x + half * 0.48f, center.y - half * 0.70f},
                                    {center.x + half, center.y});
        draw.PathBezierCubicCurveTo({center.x + half * 0.48f, center.y + half * 0.70f},
                                    {center.x - half * 0.48f, center.y + half * 0.70f},
                                    {center.x - half, center.y});
        draw.PathStroke(color, ImDrawFlags_Closed, stroke);
        draw.AddCircleFilled(center, size * 0.12f, color, 16);
        if (icon == VectorIcon::EyeOff) {
            draw.AddLine({center.x - half * 0.80f, center.y - half * 0.80f},
                         {center.x + half * 0.80f, center.y + half * 0.80f}, color,
                         stroke);
        }
    } else if (icon == VectorIcon::Back) {
        draw.AddLine({center.x - half * 0.72f, center.y},
                     {center.x + half * 0.72f, center.y}, color, stroke);
        draw.AddLine({center.x - half * 0.72f, center.y},
                     {center.x - half * 0.20f, center.y - half * 0.52f}, color, stroke);
        draw.AddLine({center.x - half * 0.72f, center.y},
                     {center.x - half * 0.20f, center.y + half * 0.52f}, color, stroke);
    } else if (icon == VectorIcon::Trash) {
        draw.AddRect({center.x - half * 0.52f, center.y - half * 0.28f},
                     {center.x + half * 0.52f, center.y + half * 0.75f}, color,
                     size * 0.08f, 0, stroke);
        draw.AddLine({center.x - half * 0.72f, center.y - half * 0.48f},
                     {center.x + half * 0.72f, center.y - half * 0.48f}, color, stroke);
        draw.AddLine({center.x - half * 0.25f, center.y - half * 0.68f},
                     {center.x + half * 0.25f, center.y - half * 0.68f}, color, stroke);
    } else if (icon == VectorIcon::Close) {
        draw.AddLine({center.x - half * 0.55f, center.y - half * 0.55f},
                     {center.x + half * 0.55f, center.y + half * 0.55f}, color, stroke);
        draw.AddLine({center.x + half * 0.55f, center.y - half * 0.55f},
                     {center.x - half * 0.55f, center.y + half * 0.55f}, color, stroke);
    } else if (icon == VectorIcon::Disconnect) {
        draw.AddLine({center.x - half * 0.78f, center.y - half * 0.24f},
                     {center.x - half * 0.20f, center.y + half * 0.34f}, color, stroke);
        draw.AddLine({center.x + half * 0.20f, center.y - half * 0.34f},
                     {center.x + half * 0.78f, center.y + half * 0.24f}, color, stroke);
        draw.AddLine({center.x - half * 0.05f, center.y - half * 0.78f},
                     {center.x - half * 0.22f, center.y - half * 0.36f}, color, stroke);
        draw.AddLine({center.x + half * 0.05f, center.y + half * 0.78f},
                     {center.x + half * 0.22f, center.y + half * 0.36f}, color, stroke);
    } else if (icon == VectorIcon::Backspace) {
        const ImVec2 points[] = {
            {center.x - half, center.y},
            {center.x - half * 0.55f, center.y - half * 0.62f},
            {center.x + half * 0.85f, center.y - half * 0.62f},
            {center.x + half * 0.85f, center.y + half * 0.62f},
            {center.x - half * 0.55f, center.y + half * 0.62f},
        };
        draw.AddPolyline(points, 5, color, ImDrawFlags_Closed, stroke);
        draw.AddLine({center.x - half * 0.05f, center.y - half * 0.28f},
                     {center.x + half * 0.42f, center.y + half * 0.28f}, color, stroke);
        draw.AddLine({center.x + half * 0.42f, center.y - half * 0.28f},
                     {center.x - half * 0.05f, center.y + half * 0.28f}, color, stroke);
    }
}

void DrawWifiSignal(ImDrawList& draw,
                    const ImVec2& center,
                    float size,
                    int signal_dbm,
                    ImU32 color) {
    const int bars = signal_dbm >= -50 ? 4 : signal_dbm >= -65 ? 3 : signal_dbm >= -78 ? 2 : 1;
    const float width = size * 0.16f;
    for (int index = 0; index < 4; ++index) {
        const float height = size * (0.25f + index * 0.18f);
        const float x = center.x - size * 0.42f + index * size * 0.25f;
        const ImU32 bar_color = index < bars ? color : (color & 0x00FFFFFFu) | 0x38000000u;
        draw.AddRectFilled({x, center.y + size * 0.42f - height},
                           {x + width, center.y + size * 0.42f}, bar_color, 2.0f);
    }
}

}  // namespace clockin
