#include "ui/components/PageDrawing.hpp"

#include "ui/Theme.hpp"

#include <algorithm>
#include <cfloat>

namespace clockin {

void PageDrawing::SetFonts(ImFont* regular, ImFont* semibold, bool icons_available) {
    regular_ = regular;
    semibold_ = semibold == nullptr ? regular : semibold;
    icons_available_ = icons_available;
}

void PageDrawing::BeginFrame(const ButtonFeedback* feedback, double now_seconds) {
    feedback_ = feedback;
    now_seconds_ = now_seconds;
}

ImFont* PageDrawing::Regular() const {
    return regular_ == nullptr ? ImGui::GetFont() : regular_;
}

ImFont* PageDrawing::SemiBold() const {
    return semibold_ == nullptr ? Regular() : semibold_;
}

ImU32 PageDrawing::WithAlpha(ImU32 color, int alpha) {
    return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
}

void PageDrawing::Surface(ImDrawList* draw,
                          const ImVec2& minimum,
                          const ImVec2& maximum,
                          float rounding,
                          bool blue_accent) const {
    draw->AddRectFilled(minimum, maximum, theme::White, rounding);
    draw->AddRect(minimum, maximum, theme::Border, rounding, 0, 1.0f);
    if (blue_accent) {
        draw->AddRectFilled({minimum.x, minimum.y + rounding},
                            {minimum.x + 5.0f, maximum.y - rounding}, theme::Blue, 2.5f);
    }
}

void PageDrawing::Text(ImDrawList* draw,
                       ImFont* font,
                       float size,
                       const ImVec2& position,
                       ImU32 color,
                       const char* text) const {
    draw->AddText(font, size, position, color, text);
}

void PageDrawing::CenteredText(ImDrawList* draw,
                               ImFont* font,
                               float size,
                               float y,
                               ImU32 color,
                               const char* text,
                               float center_x) const {
    const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    draw->AddText(font, size, {center_x - text_size.x * 0.5f, y}, color, text);
}

void PageDrawing::CenteredWrappedText(ImDrawList* draw,
                                      ImFont* font,
                                      float size,
                                      float minimum_x,
                                      float maximum_x,
                                      float y,
                                      ImU32 color,
                                      const char* text) const {
    const float width = maximum_x - minimum_x;
    const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, width, text);
    const float x = std::max(minimum_x, 400.0f - std::min(width, text_size.x) * 0.5f);
    draw->AddText(font, size, {x, y}, color, text, nullptr, width);
}

void PageDrawing::ActionButton(ImDrawList* draw,
                               const ImVec2& hit_minimum,
                               const ImVec2& hit_maximum,
                               const char* label,
                               UiControl control,
                               ButtonVariant variant,
                               VectorIcon icon) const {
    const float scale = feedback_ == nullptr ? 1.0f : feedback_->Scale(control, now_seconds_);
    const bool highlighted = feedback_ != nullptr &&
                             (feedback_->IsPressed(control) ||
                              feedback_->IsAnimating(control, now_seconds_));
    const ImVec2 center{(hit_minimum.x + hit_maximum.x) * 0.5f,
                        (hit_minimum.y + hit_maximum.y) * 0.5f};
    const ImVec2 half{(hit_maximum.x - hit_minimum.x) * 0.5f * scale,
                      (hit_maximum.y - hit_minimum.y) * 0.5f * scale};
    const ImVec2 minimum{center.x - half.x, center.y - half.y};
    const ImVec2 maximum{center.x + half.x, center.y + half.y};
    ImU32 fill = theme::White;
    ImU32 border = theme::Border;
    ImU32 text = theme::Blue;
    float border_width = 1.0f;
    if (variant == ButtonVariant::Primary) {
        fill = highlighted ? theme::BlueDark : theme::Blue;
        border = theme::BlueDark;
        text = theme::White;
    } else if (variant == ButtonVariant::Secondary) {
        fill = highlighted ? theme::PressedSurface : theme::White;
        border = theme::Blue;
        border_width = 2.0f;
    } else if (highlighted) {
        fill = theme::PressedSurface;
        border = theme::LightBlue;
    }
    draw->AddRectFilled(minimum, maximum, fill, 12.0f);
    draw->AddRect(minimum, maximum, border, 12.0f, 0, border_width);
    const ImVec2 label_size = SemiBold()->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, label);
    if (icon == VectorIcon::None) {
        draw->AddText(SemiBold(), 20.0f,
                      {center.x - label_size.x * 0.5f,
                       center.y - label_size.y * 0.5f},
                      text, label);
    } else {
        constexpr float kIconSize = 20.0f;
        constexpr float kGap = 10.0f;
        const float total_width = kIconSize + kGap + label_size.x;
        const float start_x = center.x - total_width * 0.5f;
        DrawVectorIcon(*draw, icon, {start_x + kIconSize * 0.5f, center.y},
                       kIconSize, text);
        draw->AddText(SemiBold(), 20.0f,
                      {start_x + kIconSize + kGap,
                       center.y - label_size.y * 0.5f},
                      text, label);
    }
}

void PageDrawing::IconButton(ImDrawList* draw,
                             const ImVec2& hit_minimum,
                             const ImVec2& hit_maximum,
                             VectorIcon icon,
                             UiControl control,
                             ButtonVariant variant,
                             float icon_progress) const {
    const float scale = feedback_ == nullptr ? 1.0f : feedback_->Scale(control, now_seconds_);
    const bool highlighted = feedback_ != nullptr &&
                             (feedback_->IsPressed(control) ||
                              feedback_->IsAnimating(control, now_seconds_));
    const ImVec2 center{(hit_minimum.x + hit_maximum.x) * 0.5f,
                        (hit_minimum.y + hit_maximum.y) * 0.5f};
    const float radius = std::min(hit_maximum.x - hit_minimum.x,
                                  hit_maximum.y - hit_minimum.y) * 0.5f * scale;
    ImU32 fill = highlighted ? theme::PressedSurface : theme::White;
    ImU32 border = theme::Blue;
    ImU32 icon_color = theme::Blue;
    if (variant == ButtonVariant::Primary) {
        fill = highlighted ? theme::BlueDark : theme::Blue;
        border = theme::BlueDark;
        icon_color = theme::White;
    }
    draw->AddCircleFilled(center, radius, fill, 32);
    draw->AddCircle(center, radius, border, 32, variant == ButtonVariant::Quiet ? 1.0f : 2.0f);
    DrawVectorIcon(*draw, icon, center, radius * 0.92f, icon_color, icon_progress);
}

}  // namespace clockin
