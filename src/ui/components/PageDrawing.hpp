#pragma once

#include "imgui.h"
#include "ui/components/ButtonFeedback.hpp"

namespace clockin {

enum class ButtonVariant { Primary, Secondary, Quiet };

class PageDrawing {
public:
    void SetFonts(ImFont* regular, ImFont* semibold, bool icons_available);
    void BeginFrame(const ButtonFeedback* feedback, double now_seconds);

    ImFont* Regular() const;
    ImFont* SemiBold() const;
    bool IconsAvailable() const { return icons_available_; }
    double NowSeconds() const { return now_seconds_; }

    void Surface(ImDrawList* draw,
                 const ImVec2& minimum,
                 const ImVec2& maximum,
                 float rounding,
                 bool blue_accent) const;
    void Text(ImDrawList* draw,
              ImFont* font,
              float size,
              const ImVec2& position,
              ImU32 color,
              const char* text) const;
    void CenteredText(ImDrawList* draw,
                      ImFont* font,
                      float size,
                      float y,
                      ImU32 color,
                      const char* text,
                      float center_x = 400.0f) const;
    void CenteredWrappedText(ImDrawList* draw,
                             ImFont* font,
                             float size,
                             float minimum_x,
                             float maximum_x,
                             float y,
                             ImU32 color,
                             const char* text) const;
    void ActionButton(ImDrawList* draw,
                      const ImVec2& hit_minimum,
                      const ImVec2& hit_maximum,
                      const char* label,
                      UiControl control,
                      ButtonVariant variant) const;
    static ImU32 WithAlpha(ImU32 color, int alpha);

private:
    ImFont* regular_ = nullptr;
    ImFont* semibold_ = nullptr;
    const ButtonFeedback* feedback_ = nullptr;
    double now_seconds_ = 0.0;
    bool icons_available_ = false;
};

}  // namespace clockin
