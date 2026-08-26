#pragma once

#include "app/AppEvent.hpp"

#include <string>
#include <utility>

namespace clockin {

class PageDrawing;

class OnScreenKeyboard {
public:
    UiControl HitTest(const ImVec2& point) const;
    void Activate(const AppEvent& event);
    void Clear();
    void SetError(std::string error) { error_ = std::move(error); }
    const std::string& Value() const { return value_; }
    void Render(ImDrawList& draw,
                PageDrawing& drawing,
                const std::string& ssid) const;

private:
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
