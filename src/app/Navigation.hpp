#pragma once

#include "pages/Page.hpp"

namespace clockin {

class Navigation {
public:
    PageId Current() const { return current_; }
    double EnteredAt() const { return entered_at_; }
    bool Change(PageId page, double now_seconds);

private:
    PageId current_ = PageId::Waiting;
    double entered_at_ = 0.0;
};

}  // namespace clockin
