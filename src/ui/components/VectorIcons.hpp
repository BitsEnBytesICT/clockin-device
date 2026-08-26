#pragma once

#include "imgui.h"

namespace clockin {

enum class VectorIcon {
    None,
    Card,
    Pin,
    Wifi,
    Lock,
    Globe,
    Refresh,
    Check,
    Eye,
    EyeOff,
    Back,
    Trash,
    Close,
    Disconnect,
    Backspace
};

void DrawVectorIcon(ImDrawList& draw,
                    VectorIcon icon,
                    const ImVec2& center,
                    float size,
                    ImU32 color,
                    float progress = 1.0f);
void DrawWifiSignal(ImDrawList& draw,
                    const ImVec2& center,
                    float size,
                    int signal_dbm,
                    ImU32 color);

}  // namespace clockin
