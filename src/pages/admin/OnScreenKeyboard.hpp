#pragma once

#include "app/AppEvent.hpp"
#include "ui/components/VectorIcons.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace clockin {

class PageDrawing;

struct KeyboardOptions {
    std::string title;
    std::string placeholder = "Voer het Wi-Fi-wachtwoord in";
    std::string confirm_label = "Verbinden";
    VectorIcon confirm_icon = VectorIcon::Wifi;
    bool secret = true;
    std::size_t max_length = 64;
};

class OnScreenKeyboard {
public:
    UiControl HitTest(const ImVec2& point) const;
    void Activate(const AppEvent& event);
    void Clear();
    // Clears any previous entry, then shows the keyboard with `options` and
    // `initial_value` already typed (e.g. the current server address).
    void Open(KeyboardOptions options, std::string initial_value = {});
    void SetError(std::string error) { error_ = std::move(error); }
    const std::string& Value() const { return value_; }
    void Render(ImDrawList& draw, PageDrawing& drawing) const;

private:
    KeyboardOptions options_;
    std::string value_;
    std::string error_;
    bool shift_ = false;
    bool symbols_ = false;
    bool visible_ = false;
    ImVec2 last_key_{-100.0f, -100.0f};
    double last_key_time_ = 0.0;

    char CharacterAt(const ImVec2& point) const;
};

}  // namespace clockin
