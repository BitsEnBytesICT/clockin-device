#pragma once

#include "pages/Page.hpp"

#include <string>

namespace clockin {

class AdminPinPage final : public Page {
public:
    PageId Id() const override { return PageId::AdminPin; }
    UiControl HitTest(const ImVec2& point) const override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void Reset();
    void AppendDigit(int digit, double ui_time);
    const std::string& Value() const { return value_; }

private:
    std::string value_;
    int last_digit_ = -1;
    double last_digit_time_ = 0.0;
};

}  // namespace clockin
