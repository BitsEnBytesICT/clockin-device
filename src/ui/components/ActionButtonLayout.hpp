#pragma once

#include "imgui.h"

namespace clockin {

struct ActionButtonBounds {
    ImVec2 minimum;
    ImVec2 maximum;

    bool Contains(const ImVec2& point) const {
        return point.x >= minimum.x && point.x <= maximum.x &&
               point.y >= minimum.y && point.y <= maximum.y;
    }
};

inline ActionButtonBounds CenteredRightActionButton(int slot_from_top,
                                                    int button_count) {
    constexpr float kLeft = 650.0f;
    constexpr float kRight = 790.0f;
    constexpr float kScreenHeight = 480.0f;
    constexpr float kHeight = 60.0f;
    constexpr float kSpacing = 12.0f;
    const float total_height = static_cast<float>(button_count) * kHeight +
                               static_cast<float>(button_count - 1) * kSpacing;
    const float minimum_y = (kScreenHeight - total_height) * 0.5f +
                            static_cast<float>(slot_from_top) *
                                (kHeight + kSpacing);
    return {{kLeft, minimum_y}, {kRight, minimum_y + kHeight}};
}

}  // namespace clockin
