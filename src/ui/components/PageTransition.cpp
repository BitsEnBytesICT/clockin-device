#include "ui/components/PageTransition.hpp"

#include <algorithm>

namespace clockin {
namespace {

float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float EaseOutCubic(float value) {
    const float inverse = 1.0f - Clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

}  // namespace

PageTransitionState EvaluatePageTransition(double elapsed_seconds) {
    constexpr float kDurationSeconds = 0.22f;
    const float elapsed = static_cast<float>(std::max(0.0, elapsed_seconds));
    const float eased = EaseOutCubic(elapsed / kDurationSeconds);
    return {0.35f + 0.65f * eased, 8.0f * (1.0f - eased)};
}

void ApplyPageTransition(ImDrawList& draw,
                         int first_vertex,
                         const PageTransitionState& state) {
    if (state.opacity >= 0.999f && state.offset_y <= 0.001f) return;

    const int start = std::max(0, first_vertex);
    for (int index = start; index < draw.VtxBuffer.Size; ++index) {
        ImDrawVert& vertex = draw.VtxBuffer[index];
        vertex.pos.y += state.offset_y;
        const int source_alpha = static_cast<int>((vertex.col >> 24) & 0xFFu);
        const int alpha = static_cast<int>(source_alpha * state.opacity + 0.5f);
        vertex.col = (vertex.col & 0x00FFFFFFu) |
                     (static_cast<ImU32>(alpha) << 24);
    }
}

}  // namespace clockin
