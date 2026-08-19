#pragma once

#include "pages/Page.hpp"

#include <string>
#include <utility>

namespace clockin {

class SuccessPage final : public Page {
public:
    PageId Id() const override { return PageId::Success; }
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetResult(std::string user_name, std::string action) {
        user_name_ = std::move(user_name);
        action_ = std::move(action);
    }

private:
    std::string user_name_;
    std::string action_;
};

}  // namespace clockin
