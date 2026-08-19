#pragma once

#include "pages/Page.hpp"

#include <string>
#include <utility>

namespace clockin {

class ProcessingPage final : public Page {
public:
    PageId Id() const override { return PageId::Processing; }
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void SetMessage(std::string message) { message_ = std::move(message); }

private:
    std::string message_;
};

}  // namespace clockin
