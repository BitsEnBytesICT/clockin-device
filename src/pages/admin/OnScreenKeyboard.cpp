#include "pages/admin/OnScreenKeyboard.hpp"

#include "pages/Page.hpp"
#include "ui/Theme.hpp"
#include "ui/components/PageDrawing.hpp"
#include "ui/components/VectorIcons.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <utility>

namespace clockin {
namespace {

constexpr float kRowHeight = 43.0f;
constexpr float kInnerLeft = 52.0f;
constexpr float kInnerRight = 748.0f;
constexpr float kFirstRowLeft = kInnerLeft;
constexpr float kFirstRowRight = kInnerRight;
constexpr float kSecondRowLeft = 78.0f;
constexpr float kSecondRowRight = 722.0f;
constexpr float kThirdRowLeft = 140.0f;
constexpr float kThirdRowRight = 610.0f;

char RowCharacter(const ImVec2& point,
                  float start_x,
                  float y,
                  float end_x,
                  const char* characters) {
    const int count = static_cast<int>(std::char_traits<char>::length(characters));
    if (count == 0 || point.x < start_x || point.x > end_x ||
        point.y < y || point.y > y + kRowHeight) return '\0';
    const float key_width = (end_x - start_x) / static_cast<float>(count);
    const int index = std::min(count - 1, static_cast<int>((point.x - start_x) / key_width));
    return characters[index];
}

void DrawCharacterRow(ImDrawList& draw,
                      PageDrawing& drawing,
                      float start_x,
                      float y,
                      float end_x,
                      const char* characters,
                      bool uppercase,
                      const ImVec2& last_key,
                      double key_elapsed) {
    const int count = static_cast<int>(std::char_traits<char>::length(characters));
    const float key_width = (end_x - start_x) / static_cast<float>(count);
    for (int index = 0; index < count; ++index) {
        const ImVec2 minimum{start_x + index * key_width + 2.0f, y};
        const ImVec2 maximum{start_x + (index + 1) * key_width - 2.0f,
                             y + kRowHeight};
        const bool highlighted = last_key.x >= minimum.x && last_key.x <= maximum.x &&
                                 last_key.y >= minimum.y && last_key.y <= maximum.y &&
                                 key_elapsed >= 0.0 && key_elapsed < 0.16;
        draw.AddRectFilled(minimum, maximum,
                           highlighted ? theme::PressedSurface : theme::White, 8.0f);
        draw.AddRect(minimum, maximum,
                     highlighted ? theme::Blue : theme::Border, 8.0f, 0,
                     highlighted ? 2.0f : 1.0f);
        char label[2] = {characters[index], '\0'};
        if (uppercase) label[0] = static_cast<char>(std::toupper(label[0]));
        const ImVec2 size = drawing.SemiBold()->CalcTextSizeA(17.0f, FLT_MAX, 0.0f, label);
        draw.AddText(drawing.SemiBold(), 17.0f,
                     {(minimum.x + maximum.x - size.x) * 0.5f,
                      (minimum.y + maximum.y - size.y) * 0.5f},
                     theme::Text, label);
    }
}

}  // namespace

UiControl OnScreenKeyboard::HitTest(const ImVec2& point) const {
    if (options_.secret && PointInRectangle(point, 655, 124, 44, 44)) {
        return UiControl::AdminKeyboardVisibility;
    }
    if (PointInRectangle(point, 90, 400, 240, 56)) return UiControl::AdminKeyboardCancel;
    if (PointInRectangle(point, 470, 400, 240, 56)) return UiControl::AdminKeyboardConnect;
    if (PointInRectangle(point, 52, 337, 96, 43)) return UiControl::AdminKeyboardSymbols;
    if (PointInRectangle(point, 158, 337, 590, 43)) return UiControl::AdminKeyboardSpace;
    if (PointInRectangle(point, 52, 286, 78, 43)) return UiControl::AdminKeyboardShift;
    if (PointInRectangle(point, 620, 286, 128, 43)) return UiControl::AdminKeyboardBackspace;
    return CharacterAt(point) == '\0' ? UiControl::None : UiControl::AdminKeyboardKey;
}

void OnScreenKeyboard::Activate(const AppEvent& event) {
    if (event.control == UiControl::AdminKeyboardShift) {
        shift_ = !shift_;
    } else if (event.control == UiControl::AdminKeyboardSymbols) {
        symbols_ = !symbols_;
        shift_ = false;
    } else if (event.control == UiControl::AdminKeyboardVisibility) {
        visible_ = !visible_;
    } else if (event.control == UiControl::AdminKeyboardBackspace) {
        if (!value_.empty()) value_.pop_back();
    } else if (event.control == UiControl::AdminKeyboardSpace) {
        if (value_.size() < options_.max_length) value_.push_back(' ');
    } else if (event.control == UiControl::AdminKeyboardKey &&
               value_.size() < options_.max_length) {
        char character = CharacterAt(event.position);
        if (!symbols_ && shift_) character = static_cast<char>(std::toupper(character));
        if (character != '\0') value_.push_back(character);
        shift_ = false;
    }
    last_key_ = event.position;
    last_key_time_ = event.occurred_at;
    error_.clear();
}

void OnScreenKeyboard::Clear() {
    std::fill(value_.begin(), value_.end(), '\0');
    value_.clear();
    error_.clear();
    shift_ = false;
    symbols_ = false;
    visible_ = false;
    last_key_ = {-100.0f, -100.0f};
}

void OnScreenKeyboard::Open(KeyboardOptions options, std::string initial_value) {
    Clear();
    options_ = std::move(options);
    if (initial_value.size() > options_.max_length) {
        initial_value.resize(options_.max_length);
    }
    value_ = std::move(initial_value);
}

char OnScreenKeyboard::CharacterAt(const ImVec2& point) const {
    const char* first = symbols_ ? "1234567890" : "qwertyuiop";
    const char* second = symbols_ ? "@#$%&*-_=" : "asdfghjkl";
    const char* third = symbols_ ? ".,?!:;/" : "zxcvbnm";
    char character = RowCharacter(point, kFirstRowLeft, 184, kFirstRowRight, first);
    if (character == '\0') {
        character = RowCharacter(point, kSecondRowLeft, 235, kSecondRowRight, second);
    }
    if (character == '\0') {
        character = RowCharacter(point, kThirdRowLeft, 286, kThirdRowRight, third);
    }
    return character;
}

void OnScreenKeyboard::Render(ImDrawList& draw, PageDrawing& drawing) const {
    draw.AddRectFilled({35, 78}, {765, 468}, theme::OffWhite, 14.0f);
    draw.AddRect({35, 78}, {765, 468}, theme::Border, 14.0f);
    drawing.CenteredText(&draw, drawing.SemiBold(), 19.0f, 88.0f, theme::Text,
                         options_.title.c_str());
    const bool masked = options_.secret && !visible_;
    std::string display = masked ? std::string(value_.size(), '*') : value_;
    // Keep the end of long entries (where typing happens) in view.
    constexpr float kFieldFontSize = 25.0f;
    const float available_width = options_.secret ? 520.0f : 580.0f;
    const float center_x = options_.secret ? 375.0f : 400.0f;
    ImFont* font = drawing.SemiBold();
    if (font->CalcTextSizeA(kFieldFontSize, FLT_MAX, 0.0f, display.c_str()).x >
        available_width) {
        std::size_t start = 0;
        while (start < display.size() &&
               font->CalcTextSizeA(kFieldFontSize, FLT_MAX, 0.0f,
                                   ("..." + display.substr(start)).c_str()).x >
                   available_width) {
            ++start;
        }
        display = "..." + display.substr(start);
    }
    draw.AddRectFilled({90, 118}, {710, 174}, theme::White, 10.0f);
    draw.AddRect({90, 118}, {710, 174}, theme::LightBlue, 10.0f, 0, 1.5f);
    drawing.CenteredText(&draw, font, kFieldFontSize, 132.0f,
                         display.empty() ? theme::MutedText : theme::Text,
                         display.empty() ? options_.placeholder.c_str() : display.c_str(),
                         center_x);
    if (!display.empty()) {
        const float width =
            font->CalcTextSizeA(kFieldFontSize, FLT_MAX, 0.0f, display.c_str()).x;
        const float caret_x = center_x + width * 0.5f + 3.0f;
        const float blink = static_cast<float>(
            0.5 + 0.5 * std::cos(std::fmod(drawing.NowSeconds() * 6.0, 6.283185307179586)));
        draw.AddRectFilled({caret_x, 133.0f}, {caret_x + 2.0f, 160.0f},
                           PageDrawing::WithAlpha(theme::Blue,
                                                  static_cast<int>(70.0f + 185.0f * blink)));
    }
    if (options_.secret) {
        drawing.IconButton(&draw, {655, 124}, {699, 168},
                           visible_ ? VectorIcon::EyeOff : VectorIcon::Eye,
                           UiControl::AdminKeyboardVisibility, ButtonVariant::Quiet);
    }
    const double elapsed = drawing.NowSeconds() - last_key_time_;
    DrawCharacterRow(draw, drawing, kFirstRowLeft, 184, kFirstRowRight,
                     symbols_ ? "1234567890" : "qwertyuiop", shift_, last_key_, elapsed);
    DrawCharacterRow(draw, drawing, kSecondRowLeft, 235, kSecondRowRight,
                     symbols_ ? "@#$%&*-_=" : "asdfghjkl", shift_, last_key_, elapsed);
    DrawCharacterRow(draw, drawing, kThirdRowLeft, 286, kThirdRowRight,
                     symbols_ ? ".,?!:;/" : "zxcvbnm", shift_, last_key_, elapsed);
    drawing.ActionButton(&draw, {52, 286}, {130, 329}, shift_ ? "SHIFT" : "Shift",
                         UiControl::AdminKeyboardShift, ButtonVariant::Secondary);
    drawing.ActionButton(&draw, {620, 286}, {748, 329}, "Verwijder",
                         UiControl::AdminKeyboardBackspace, ButtonVariant::Secondary,
                         VectorIcon::Backspace);
    drawing.ActionButton(&draw, {52, 337}, {148, 380}, symbols_ ? "ABC" : "123?",
                         UiControl::AdminKeyboardSymbols, ButtonVariant::Secondary);
    drawing.ActionButton(&draw, {158, 337}, {748, 380}, "Spatie",
                         UiControl::AdminKeyboardSpace, ButtonVariant::Secondary);
    drawing.ActionButton(&draw, {90, 400}, {330, 456}, "Annuleren",
                         UiControl::AdminKeyboardCancel, ButtonVariant::Secondary,
                         VectorIcon::Close);
    drawing.ActionButton(&draw, {470, 400}, {710, 456}, options_.confirm_label.c_str(),
                         UiControl::AdminKeyboardConnect, ButtonVariant::Primary,
                         options_.confirm_icon);
    if (!error_.empty()) {
        drawing.CenteredText(&draw, drawing.Regular(), 13.0f, 382.0f,
                             theme::RedText, error_.c_str());
    }
}

}  // namespace clockin
