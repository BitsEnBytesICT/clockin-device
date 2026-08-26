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
    bool BeginScroll(const ImVec2& point) override;
    void ScrollBy(float delta_y) override;
    void EndScroll() override { dragging_ = false; }
    void Reset();

private:
    std::string user_name_;
    std::vector<std::string> dates_;
    std::string warning_;
    float scroll_offset_ = 0.0f;
    bool dragging_ = false;
};

}  // namespace clockin
