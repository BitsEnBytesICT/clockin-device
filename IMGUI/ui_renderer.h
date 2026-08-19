#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "imgui.h"
#include "ui_feedback.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace BitsBytesTheme {
static const ImU32 Orange = IM_COL32(242, 136, 15, 255);
static const ImU32 Blue = IM_COL32(49, 113, 153, 255);
static const ImU32 BlueDark = IM_COL32(39, 91, 124, 255);
static const ImU32 LightBlue = IM_COL32(146, 213, 255, 255);
static const ImU32 White = IM_COL32(255, 255, 255, 255);
static const ImU32 LightWhite = IM_COL32(243, 248, 253, 255);
static const ImU32 OffWhite = IM_COL32(247, 251, 255, 255);
static const ImU32 Text = IM_COL32(65, 65, 65, 255);
static const ImU32 MutedText = IM_COL32(103, 117, 128, 255);
static const ImU32 Border = IM_COL32(220, 232, 241, 255);
static const ImU32 Red = IM_COL32(255, 107, 107, 255);
static const ImU32 RedText = IM_COL32(176, 54, 54, 255);
static const ImU32 Green = IM_COL32(96, 243, 118, 255);
static const ImU32 GreenText = IM_COL32(30, 122, 54, 255);
static const ImU32 PressedSurface = IM_COL32(230, 244, 252, 255);
}

enum class UIButtonVariant {
    Primary,
    Secondary,
    Quiet
};

class UIRenderer {
public:
    UIRenderer()
        : regular_(NULL), semibold_(NULL), feedback_(NULL), now_seconds_(0.0),
          icons_available_(false) {}

    void SetFonts(ImFont* regular, ImFont* semibold, bool icons_available) {
        regular_ = regular;
        semibold_ = semibold != NULL ? semibold : regular;
        icons_available_ = icons_available;
    }

    void BeginFrame(const UIButtonFeedback* feedback, double now_seconds) {
        feedback_ = feedback;
        now_seconds_ = now_seconds;
    }

    void RenderWaitingScreen(ImDrawList* draw_list) {
        DrawSurface(draw_list, ImVec2(60, 120), ImVec2(740, 410), 16.0f, true);

        const ImVec2 center(400.0f, 220.0f);
        const float time = static_cast<float>(ImGui::GetTime());
        const float period = 1.0f;
        for (int ring = 0; ring < 2; ++ring) {
            float phase = std::fmod(time - ring * 0.5f, period);
            if (phase < 0.0f) phase += period;
            if (phase < 0.05f) continue;
            const float radius = 7.0f + 53.0f * phase;
            const int alpha = static_cast<int>((1.0f - phase) * 220.0f);
            draw_list->AddCircle(center, radius, WithAlpha(BitsBytesTheme::Blue, alpha), 32, 4.0f);
        }
        draw_list->AddCircleFilled(center, 9.0f, BitsBytesTheme::Orange, 20);

        DrawCenteredText(draw_list, SemiBold(), 32.0f, 304.0f,
                         BitsBytesTheme::Text, "Scan uw RFID-kaart");
        DrawCenteredText(draw_list, Regular(), 20.0f, 350.0f,
                         BitsBytesTheme::MutedText, "Houd uw kaart voor de lezer");

        DrawActionButton(draw_list,
                         ImVec2(650, 10), ImVec2(790, 70),
                         icons_available_ ? u8"\uE000Beheer" : "Beheer",
                         UIControl::Admin,
                         UIButtonVariant::Quiet);
    }

    void RenderAdminPasswordScreen(ImDrawList* draw_list,
                                   const std::string& password_buffer,
                                   int last_digit,
                                   float last_digit_time) {
        DrawSurface(draw_list, ImVec2(40, 40), ImVec2(760, 450), 14.0f, true);
        DrawCenteredText(draw_list, SemiBold(), 28.0f, 67.0f,
                         BitsBytesTheme::Text, "Voer beheerders-pincode in");
        DrawCenteredText(draw_list, Regular(), 18.0f, 108.0f,
                         BitsBytesTheme::MutedText, "Pincode");

        const float dot_start = 400.0f - 3.0f * 17.0f;
        for (int index = 0; index < 4; ++index) {
            const ImVec2 center(dot_start + index * 34.0f, 151.0f);
            if (index < static_cast<int>(password_buffer.size())) {
                draw_list->AddCircleFilled(center, 6.0f, BitsBytesTheme::Blue, 20);
            } else {
                draw_list->AddCircle(center, 6.0f, BitsBytesTheme::LightBlue, 20, 2.0f);
            }
        }

        const float now = static_cast<float>(ImGui::GetTime());
        for (int digit = 1; digit <= 9; ++digit) {
            const int row = (digit - 1) / 3;
            const int column = (digit - 1) % 3;
            const float button_x = KEYPAD_START_X + column * KEYPAD_SPACING_X;
            const float button_y = KEYPAD_START_Y + row * KEYPAD_SPACING_Y;
            const ImVec2 center(button_x + KEYPAD_BUTTON_W * 0.5f,
                                button_y + KEYPAD_BUTTON_H * 0.5f);
            const float base_radius = 38.0f;
            float scale = 1.0f;
            const float elapsed = now - last_digit_time;
            if (digit == last_digit && elapsed >= 0.0f && elapsed < 0.18f) {
                const float remaining = 1.0f - elapsed / 0.18f;
                scale = 1.0f + 0.12f * remaining;
            }
            const bool highlighted = digit == last_digit && elapsed >= 0.0f && elapsed < 0.18f;
            draw_list->AddCircleFilled(center,
                                       base_radius * scale,
                                       highlighted ? BitsBytesTheme::PressedSurface : BitsBytesTheme::White,
                                       32);
            draw_list->AddCircle(center,
                                 base_radius * scale,
                                 highlighted ? BitsBytesTheme::Blue : BitsBytesTheme::LightBlue,
                                 32,
                                 highlighted ? 2.5f : 1.5f);

            char digit_text[2] = {static_cast<char>('0' + digit), '\0'};
            const ImVec2 digit_size = SemiBold()->CalcTextSizeA(
                22.0f, FLT_MAX, 0.0f, digit_text);
            draw_list->AddText(SemiBold(), 22.0f,
                               ImVec2(center.x - digit_size.x * 0.5f,
                                      center.y - digit_size.y * 0.5f),
                               highlighted ? BitsBytesTheme::Blue : BitsBytesTheme::Text,
                               digit_text);
        }

        DrawActionButton(draw_list,
                         ImVec2(650, 10), ImVec2(790, 70),
                         icons_available_ ? u8"\uE004Terug" : "Terug",
                         UIControl::AdminBack,
                         UIButtonVariant::Secondary);
    }

    void RenderAdminScreen(ImDrawList* draw_list, const std::string& uid) {
        DrawCenteredText(draw_list, SemiBold(), 30.0f, 34.0f,
                         BitsBytesTheme::Text, "Beheermenu");
        DrawSurface(draw_list, ImVec2(90, 105), ImVec2(610, 310), 14.0f, true);
        DrawText(draw_list, SemiBold(), 18.0f, ImVec2(125, 145),
                 BitsBytesTheme::Blue, "Laatst gelezen kaartnummer");
        DrawText(draw_list, Regular(), 26.0f, ImVec2(125, 190),
                 BitsBytesTheme::Text, uid.empty() ? "Nog geen kaart gelezen" : uid.c_str());
        DrawText(draw_list, Regular(), 17.0f, ImVec2(125, 250),
                 BitsBytesTheme::MutedText, "Scan een kaart om het kaartnummer te controleren.");

        DrawActionButton(draw_list,
                         ImVec2(650, 10), ImVec2(790, 70),
                         icons_available_ ? u8"\uE004Terug" : "Terug",
                         UIControl::AdminBack,
                         UIButtonVariant::Secondary);
    }

    void RenderAttendanceScreen(ImDrawList* draw_list,
                                const std::string& user_name,
                                const std::vector<std::string>& dates,
                                const std::string& warning,
                                float scroll_offset) {
        DrawCenteredText(draw_list, SemiBold(), 27.0f, 20.0f,
                         BitsBytesTheme::Text, "Aanwezigheid", 320.0f);

        char user_label[192];
        snprintf(user_label, sizeof(user_label), "Gebruiker: %s",
                 user_name.empty() ? "(onbekend)" : user_name.c_str());
        DrawText(draw_list, Regular(), 19.0f, ImVec2(40, 60),
                 BitsBytesTheme::MutedText, user_label);
        if (!warning.empty()) {
            DrawText(draw_list, Regular(), 17.0f, ImVec2(40, 80),
                     BitsBytesTheme::RedText, warning.c_str());
        }

        const ImVec2 table_min(40, 90);
        const ImVec2 table_max(600, 420);
        DrawSurface(draw_list, table_min, table_max, 12.0f, true);
        draw_list->AddRectFilled(ImVec2(45, 91), ImVec2(599, 136),
                                 BitsBytesTheme::LightWhite, 11.0f,
                                 ImDrawFlags_RoundCornersTopRight);
        DrawText(draw_list, SemiBold(), 18.0f, ImVec2(62, 107),
                 BitsBytesTheme::Blue, "Laatste 30 dagen");
        draw_list->AddLine(ImVec2(50, 136), ImVec2(590, 136),
                           BitsBytesTheme::Border, 1.0f);

        const float start_y = 146.0f;
        const float row_height = 24.0f;
        const float visible_height = table_max.y - start_y;
        const float total_height = row_height * static_cast<float>(dates.size());
        const float max_scroll = total_height > visible_height
            ? total_height - visible_height
            : 0.0f;
        scroll_offset = std::max(-max_scroll, std::min(0.0f, scroll_offset));

        if (dates.empty()) {
            DrawText(draw_list, Regular(), 18.0f, ImVec2(62, 155),
                     BitsBytesTheme::MutedText,
                     "Geen aanwezigheid in de laatste 30 dagen");
        } else {
            draw_list->PushClipRect(ImVec2(table_min.x + 5.0f, start_y), table_max, true);
            for (size_t index = 0; index < dates.size(); ++index) {
                const float y = start_y + static_cast<float>(index) * row_height + scroll_offset;
                if (y < start_y - row_height || y > table_max.y) continue;
                DrawText(draw_list, Regular(), 18.0f, ImVec2(62, y),
                         BitsBytesTheme::Text, dates[index].c_str());
            }
            draw_list->PopClipRect();

            if (max_scroll > 0.0f) {
                const float bar_min_x = table_max.x + 8.0f;
                const float bar_max_x = table_max.x + 18.0f;
                const float bar_min_y = start_y;
                const float bar_max_y = table_max.y;
                draw_list->AddRectFilled(ImVec2(bar_min_x, bar_min_y),
                                         ImVec2(bar_max_x, bar_max_y),
                                         BitsBytesTheme::Border, 5.0f);
                const float thumb_height = std::max(
                    24.0f, (visible_height / total_height) * (bar_max_y - bar_min_y));
                const float scroll_t = std::max(0.0f, std::min(1.0f, -scroll_offset / max_scroll));
                const float thumb_y = bar_min_y +
                    (bar_max_y - bar_min_y - thumb_height) * scroll_t;
                draw_list->AddRectFilled(ImVec2(bar_min_x + 1.0f, thumb_y),
                                         ImVec2(bar_max_x - 1.0f, thumb_y + thumb_height),
                                         BitsBytesTheme::Blue, 4.0f);
            }
        }

        DrawActionButton(draw_list,
                         ImVec2(650, 10), ImVec2(790, 70),
                         icons_available_ ? u8"\uE004Terug" : "Terug",
                         UIControl::AttendanceBack,
                         UIButtonVariant::Secondary);
        DrawActionButton(draw_list,
                         ImVec2(650, 400), ImVec2(790, 460),
                         icons_available_ ? u8"\uE005Bevestig" : "Bevestig",
                         UIControl::AttendanceConfirm,
                         UIButtonVariant::Primary);
    }

    void RenderSignatureScreen(ImDrawList* draw_list,
                               const std::string& user_name,
                               const std::vector<std::vector<ImVec2> >& strokes,
                               const std::vector<ImVec2>& current_stroke,
                               const std::string& warning) {
        char header[192];
        snprintf(header, sizeof(header), "Welkom, %s!", user_name.c_str());
        DrawCenteredText(draw_list, SemiBold(), 28.0f, 28.0f,
                         BitsBytesTheme::Text, header, 320.0f);
        DrawCenteredText(draw_list, Regular(), 19.0f, 78.0f,
                         BitsBytesTheme::MutedText,
                         "Zet hieronder uw handtekening om in te klokken",
                         320.0f);
        if (!warning.empty()) {
            DrawCenteredWrappedText(draw_list, Regular(), 17.0f,
                                    45.0f, 595.0f, 108.0f,
                                    BitsBytesTheme::RedText, warning.c_str());
        }

        const ImVec2 signature_min(50, 150);
        const ImVec2 signature_max(600, 420);
        DrawSurface(draw_list, signature_min, signature_max, 10.0f, false);
        draw_list->AddLine(ImVec2(85, 380), ImVec2(565, 380),
                           BitsBytesTheme::Border, 1.0f);

        for (size_t index = 0; index < strokes.size(); ++index) {
            if (strokes[index].size() > 1) {
                draw_list->AddPolyline(strokes[index].data(),
                                       static_cast<int>(strokes[index].size()),
                                       BitsBytesTheme::Text, false, 3.0f);
            }
        }
        if (current_stroke.size() > 1) {
            draw_list->AddPolyline(current_stroke.data(),
                                   static_cast<int>(current_stroke.size()),
                                   BitsBytesTheme::Text, false, 3.0f);
        }

        DrawActionButton(draw_list,
                         ImVec2(650, 10), ImVec2(790, 70),
                         icons_available_ ? u8"\uE001Wissen" : "Wissen",
                         UIControl::SignatureClear,
                         UIButtonVariant::Quiet);
        DrawActionButton(draw_list,
                         ImVec2(650, 205), ImVec2(790, 265),
                         icons_available_ ? u8"\uE002Annuleren" : "Annuleren",
                         UIControl::SignatureCancel,
                         UIButtonVariant::Secondary);
        DrawActionButton(draw_list,
                         ImVec2(650, 400), ImVec2(790, 460),
                         icons_available_ ? u8"\uE003Versturen" : "Versturen",
                         UIControl::SignatureSubmit,
                         UIButtonVariant::Primary);
    }

    void RenderProcessingScreen(ImDrawList* draw_list, const std::string& message) {
        DrawSurface(draw_list, ImVec2(165, 95), ImVec2(635, 390), 16.0f, true);
        const ImVec2 center(400.0f, 215.0f);
        const float start_angle = static_cast<float>(ImGui::GetTime()) * 5.0f;
        const int segments = 24;
        for (int segment = 0; segment < segments; ++segment) {
            const float phase = static_cast<float>(segment) / segments;
            const float angle_start = start_angle + phase * 6.2831853f;
            const float angle_end = start_angle + (phase + 0.7f / segments) * 6.2831853f;
            const int alpha = 45 + static_cast<int>(phase * 210.0f);
            draw_list->AddLine(
                ImVec2(center.x + std::cos(angle_start) * 48.0f,
                       center.y + std::sin(angle_start) * 48.0f),
                ImVec2(center.x + std::cos(angle_end) * 48.0f,
                       center.y + std::sin(angle_end) * 48.0f),
                WithAlpha(BitsBytesTheme::Blue, alpha), 4.0f);
        }
        DrawCenteredWrappedText(draw_list, SemiBold(), 27.0f,
                                205.0f, 595.0f, 305.0f,
                                BitsBytesTheme::Text,
                                message.empty() ? "Even geduld..." : message.c_str());
    }

    void RenderSuccessScreen(ImDrawList* draw_list,
                             const std::string& user_name,
                             const std::string& action) {
        DrawSurface(draw_list, ImVec2(145, 75), ImVec2(655, 410), 16.0f, true);
        const ImVec2 center(400, 190);
        draw_list->AddCircleFilled(center, 60.0f, BitsBytesTheme::Green, 40);
        draw_list->AddLine(ImVec2(center.x - 25, center.y),
                           ImVec2(center.x - 5, center.y + 20),
                           BitsBytesTheme::GreenText, 6.0f);
        draw_list->AddLine(ImVec2(center.x - 5, center.y + 20),
                           ImVec2(center.x + 30, center.y - 25),
                           BitsBytesTheme::GreenText, 6.0f);

        const char* message = action == "clock_in"
            ? "Succesvol ingeklokt!"
            : "Succesvol uitgeklokt!";
        DrawCenteredText(draw_list, SemiBold(), 31.0f, 285.0f,
                         BitsBytesTheme::Text, message);
        DrawCenteredWrappedText(draw_list, Regular(), 23.0f,
                                180.0f, 620.0f, 340.0f,
                                BitsBytesTheme::MutedText, user_name.c_str());
    }

    void RenderErrorScreen(ImDrawList* draw_list,
                           const std::string& message,
                           const std::string& detail = std::string()) {
        DrawSurface(draw_list, ImVec2(110, 65), ImVec2(690, 420), 16.0f, true);
        const ImVec2 center(400, 180);
        draw_list->AddCircleFilled(center, 58.0f, BitsBytesTheme::Red, 40);
        draw_list->AddLine(ImVec2(center.x - 20, center.y - 20),
                           ImVec2(center.x + 20, center.y + 20),
                           BitsBytesTheme::White, 6.0f);
        draw_list->AddLine(ImVec2(center.x + 20, center.y - 20),
                           ImVec2(center.x - 20, center.y + 20),
                           BitsBytesTheme::White, 6.0f);

        DrawCenteredWrappedText(draw_list, SemiBold(), 27.0f,
                                145.0f, 655.0f, 275.0f,
                                BitsBytesTheme::RedText, message.c_str());
        if (!detail.empty()) {
            DrawCenteredWrappedText(draw_list, Regular(), 19.0f,
                                    145.0f, 655.0f, 335.0f,
                                    BitsBytesTheme::MutedText, detail.c_str());
        }
    }

private:
    ImFont* regular_;
    ImFont* semibold_;
    const UIButtonFeedback* feedback_;
    double now_seconds_;
    bool icons_available_;

    ImFont* Regular() const {
        return regular_ != NULL ? regular_ : ImGui::GetFont();
    }

    ImFont* SemiBold() const {
        return semibold_ != NULL ? semibold_ : Regular();
    }

    static ImU32 WithAlpha(ImU32 color, int alpha) {
        return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
    }

    void DrawSurface(ImDrawList* draw_list,
                     const ImVec2& min,
                     const ImVec2& max,
                     float rounding,
                     bool blue_accent) const {
        draw_list->AddRectFilled(min, max, BitsBytesTheme::White, rounding);
        draw_list->AddRect(min, max, BitsBytesTheme::Border, rounding, 0, 1.0f);
        if (blue_accent) {
            draw_list->AddRectFilled(ImVec2(min.x, min.y + rounding),
                                     ImVec2(min.x + 5.0f, max.y - rounding),
                                     BitsBytesTheme::Blue, 2.5f);
        }
    }

    void DrawText(ImDrawList* draw_list,
                  ImFont* font,
                  float size,
                  const ImVec2& position,
                  ImU32 color,
                  const char* text) const {
        draw_list->AddText(font, size, position, color, text);
    }

    void DrawCenteredText(ImDrawList* draw_list,
                          ImFont* font,
                          float size,
                          float y,
                          ImU32 color,
                          const char* text,
                          float center_x = 400.0f) const {
        const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        draw_list->AddText(font, size,
                           ImVec2(center_x - text_size.x * 0.5f, y),
                           color, text);
    }

    void DrawCenteredWrappedText(ImDrawList* draw_list,
                                 ImFont* font,
                                 float size,
                                 float min_x,
                                 float max_x,
                                 float y,
                                 ImU32 color,
                                 const char* text) const {
        const float width = max_x - min_x;
        const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, width, text);
        const float x = std::max(min_x, 400.0f - std::min(width, text_size.x) * 0.5f);
        draw_list->AddText(font, size, ImVec2(x, y), color, text, NULL, width);
    }

    void DrawActionButton(ImDrawList* draw_list,
                          const ImVec2& hit_min,
                          const ImVec2& hit_max,
                          const char* label,
                          UIControl control,
                          UIButtonVariant variant) const {
        const float scale = feedback_ != NULL
            ? feedback_->Scale(control, now_seconds_)
            : 1.0f;
        const bool highlighted = feedback_ != NULL &&
            (feedback_->IsPressed(control) || feedback_->IsAnimating(control, now_seconds_));
        const ImVec2 center((hit_min.x + hit_max.x) * 0.5f,
                            (hit_min.y + hit_max.y) * 0.5f);
        const ImVec2 half((hit_max.x - hit_min.x) * 0.5f * scale,
                          (hit_max.y - hit_min.y) * 0.5f * scale);
        const ImVec2 min(center.x - half.x, center.y - half.y);
        const ImVec2 max(center.x + half.x, center.y + half.y);

        ImU32 fill = BitsBytesTheme::White;
        ImU32 border = BitsBytesTheme::Border;
        ImU32 text = BitsBytesTheme::Blue;
        float border_width = 1.0f;
        if (variant == UIButtonVariant::Primary) {
            fill = highlighted ? BitsBytesTheme::BlueDark : BitsBytesTheme::Blue;
            border = BitsBytesTheme::BlueDark;
            text = BitsBytesTheme::White;
        } else if (variant == UIButtonVariant::Secondary) {
            fill = highlighted ? BitsBytesTheme::PressedSurface : BitsBytesTheme::White;
            border = BitsBytesTheme::Blue;
            border_width = 2.0f;
        } else if (highlighted) {
            fill = BitsBytesTheme::PressedSurface;
            border = BitsBytesTheme::LightBlue;
        }

        draw_list->AddRectFilled(min, max, fill, 12.0f);
        draw_list->AddRect(min, max, border, 12.0f, 0, border_width);
        const ImVec2 label_size = SemiBold()->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, label);
        draw_list->AddText(SemiBold(), 20.0f,
                           ImVec2(center.x - label_size.x * 0.5f,
                                  center.y - label_size.y * 0.5f),
                           text, label);
    }
};

#endif
