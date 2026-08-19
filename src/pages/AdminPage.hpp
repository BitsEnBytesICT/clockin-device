#pragma once

#include "pages/Page.hpp"

#include <string>
#include <utility>

namespace clockin {

class AdminPage final : public Page {
public:
    PageId Id() const override { return PageId::Admin; }
    UiControl HitTest(const ImVec2& point) const override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetUid(std::string uid) { uid_ = std::move(uid); }
    void Reset() { uid_.clear(); }

private:
    std::string uid_;
};

}  // namespace clockin
