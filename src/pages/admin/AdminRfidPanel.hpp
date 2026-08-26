#pragma once

#include <string>

struct ImDrawList;

namespace clockin {

class PageDrawing;

class AdminRfidPanel {
public:
    void SetUid(std::string uid);
    void Reset();
    void Render(ImDrawList& draw, PageDrawing& drawing) const;

private:
    std::string uid_;
    double read_at_ = 0.0;
};

}  // namespace clockin
