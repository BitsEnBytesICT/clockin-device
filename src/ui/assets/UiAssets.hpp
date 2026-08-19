#pragma once

#include "imgui.h"

namespace clockin {

struct FontBundle {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    bool icons_installed = false;
};

namespace icons {
inline constexpr ImWchar Settings = 0xE000;
inline constexpr ImWchar Delete = 0xE001;
inline constexpr ImWchar Close = 0xE002;
inline constexpr ImWchar Signature = 0xE003;
inline constexpr ImWchar Back = 0xE004;
inline constexpr ImWchar Check = 0xE005;
}  // namespace icons

FontBundle AddBrandedFonts(ImGuiIO& io, float size_pixels);
void InstallBrandedIcons(ImGuiIO& io, FontBundle& bundle, float size_pixels);

}  // namespace clockin
