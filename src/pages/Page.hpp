#pragma once

#include "imgui.h"
#include "ui/components/UiControl.hpp"

namespace clockin {

class PageDrawing;

enum class PageId {
    Waiting,
    Processing,
    Attendance,
    Signature,
    Success,
    Error,
    AdminPin,
    Admin
};

class Page {
public:
    virtual ~Page() = default;
    virtual PageId Id() const = 0;
    virtual UiControl HitTest(const ImVec2&) const { return UiControl::None; }
    virtual bool BeginScroll(const ImVec2&) { return false; }
    virtual void ScrollBy(float) {}
    virtual void EndScroll() {}
    virtual void Render(ImDrawList& draw, PageDrawing& drawing) const = 0;
};

inline bool PointInRectangle(const ImVec2& point,
                             float x,
                             float y,
                             float width,
                             float height) {
    return point.x >= x && point.x <= x + width && point.y >= y &&
           point.y <= y + height;
}

}  // namespace clockin
