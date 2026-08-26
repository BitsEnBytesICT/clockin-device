#pragma once

#include "app/AppEvent.hpp"
#include "pages/admin/AdminAction.hpp"
#include "pages/admin/OnScreenKeyboard.hpp"
#include "services/network/NetworkTypes.hpp"

#include <string>

namespace clockin {

class PageDrawing;

class AdminNetworkPanel {
public:
    void Reset();
    void SetSnapshot(NetworkSnapshot snapshot);
    void SetApiStatus(bool known, bool online);
    void ShowCommandError(std::string error);
    UiControl HitTest(const ImVec2& point) const;
    AdminAction Activate(const AppEvent& event);
    bool BeginScroll(const ImVec2& point);
    void ScrollBy(float delta_y);
    void EndScroll() { dragging_ = false; }
    bool HasSubflow() const;
    bool KeyboardOpen() const { return mode_ == Mode::Password; }
    void Back(double now_seconds);
    void ClearSecrets();
    void Render(ImDrawList& draw, PageDrawing& drawing) const;

private:
    enum class Mode { List, Details, Password, ConfirmForget };

    NetworkSnapshot snapshot_;
    Mode mode_ = Mode::List;
    std::string selected_ssid_;
    std::string local_error_;
    OnScreenKeyboard keyboard_;
    bool api_known_ = false;
    bool api_online_ = false;
    bool dragging_ = false;
    float scroll_offset_ = 0.0f;
    double mode_changed_at_ = 0.0;

    const WifiNetwork* Selected() const;
    int NetworkIndexAt(const ImVec2& point) const;
    bool Busy() const;
    void ClampScroll();
    void RenderList(ImDrawList& draw, PageDrawing& drawing) const;
    void RenderDetails(ImDrawList& draw, PageDrawing& drawing) const;
    void RenderConfirmation(ImDrawList& draw, PageDrawing& drawing) const;
};

}  // namespace clockin
