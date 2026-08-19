#pragma once

#include "pages/Page.hpp"

#include <string>
#include <vector>

namespace clockin {

class AttendancePage final : public Page {
public:
    PageId Id() const override { return PageId::Attendance; }
    UiControl HitTest(const ImVec2& point) const override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetData(std::string user_name,
                 std::vector<std::string> dates,
                 std::string warning);
    void BeginDrag() { dragging_ = true; }
    void Drag(float delta_y);
    void EndDrag() { dragging_ = false; }
    bool Dragging() const { return dragging_; }
    void Reset();

private:
    std::string user_name_;
    std::vector<std::string> dates_;
    std::string warning_;
    float scroll_offset_ = 0.0f;
    bool dragging_ = false;
};

}  // namespace clockin
