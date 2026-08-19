#pragma once

#include "pages/Page.hpp"

#include <string>
#include <utility>

namespace clockin {

class ErrorPage final : public Page {
public:
    PageId Id() const override { return PageId::Error; }
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetError(std::string message, std::string detail) {
        message_ = std::move(message);
        detail_ = std::move(detail);
    }

private:
    std::string message_;
    std::string detail_;
};

}  // namespace clockin
