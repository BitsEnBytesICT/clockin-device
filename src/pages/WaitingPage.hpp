#pragma once

#include "pages/Page.hpp"

namespace clockin {

class WaitingPage final : public Page {
public:
    PageId Id() const override { return PageId::Waiting; }
    UiControl HitTest(const ImVec2& point) const override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetApiStatus(bool known, bool online) {
        api_known_ = known;
        api_online_ = online;
    }

private:
    bool api_known_ = false;
    bool api_online_ = false;
};

}  // namespace clockin
