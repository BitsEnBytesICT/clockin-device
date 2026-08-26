#pragma once

#include "pages/Page.hpp"

#include <string>
#include <utility>

namespace clockin {

class ErrorPage final : public Page {
public:
    PageId Id() const override { return PageId::Error; }
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetError(std::string message,
                  std::string detail,
                  double animation_started_at) {
        message_ = std::move(message);
        detail_ = std::move(detail);
        animation_started_at_ = animation_started_at;
    }

private:
    std::string message_;
    std::string detail_;
    double animation_started_at_ = 0.0;
};

}  // namespace clockin
