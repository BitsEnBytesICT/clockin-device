#include "app/Navigation.hpp"

namespace clockin {

bool Navigation::Change(PageId page, double now_seconds) {
    if (current_ == page) return false;
    current_ = page;
    entered_at_ = now_seconds;
    return true;
}

}  // namespace clockin
