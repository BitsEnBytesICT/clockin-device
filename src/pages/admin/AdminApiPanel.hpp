#pragma once

#include "app/AppEvent.hpp"
#include "pages/admin/AdminAction.hpp"
#include "pages/admin/OnScreenKeyboard.hpp"

#include <string>
#include <utility>

namespace clockin {

class PageDrawing;

// Shows the management server address and lets an administrator change it
// with the same touch keyboard the Wi-Fi password entry uses.
class AdminApiPanel {
public:
    void Reset();
    void SetCurrentUrl(std::string url) { current_url_ = std::move(url); }
    void SetApiStatus(bool known, bool online);
    UiControl HitTest(const ImVec2& point) const;
    AdminAction Activate(const AppEvent& event);
    void CompleteSave(bool succeeded, const std::string& error, double now_seconds);
    bool HasSubflow() const { return mode_ == Mode::Edit; }
    bool KeyboardOpen() const { return mode_ == Mode::Edit; }
    void Cancel(double now_seconds);
    void Render(ImDrawList& draw, PageDrawing& drawing) const;

private:
    enum class Mode { Overview, Edit };

    Mode mode_ = Mode::Overview;
    std::string current_url_;
    std::string message_;
    bool message_is_error_ = false;
    bool api_known_ = false;
    bool api_online_ = false;
    OnScreenKeyboard keyboard_;
    double mode_changed_at_ = 0.0;

    void BeginEdit(double now_seconds);
};

}  // namespace clockin
