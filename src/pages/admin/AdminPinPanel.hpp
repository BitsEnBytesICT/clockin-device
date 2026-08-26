#pragma once

#include "app/AppEvent.hpp"
#include "pages/admin/AdminAction.hpp"

#include <string>

namespace clockin {

class PageDrawing;

class AdminPinPanel {
public:
    void Reset(bool persistable);
    UiControl HitTest(const ImVec2& point) const;
    AdminAction Activate(const AppEvent& event);
    void CompleteSave(bool succeeded, const std::string& message, double now_seconds);
    bool HasSubflow() const;
    void CancelSubflow(double now_seconds);
    void ClearSecrets();
    void Render(ImDrawList& draw, PageDrawing& drawing) const;

private:
    enum class Mode { Overview, EnterNew, Confirm, Saving, Saved };

    Mode mode_ = Mode::Overview;
    bool persistable_ = true;
    std::string first_pin_;
    std::string confirm_pin_;
    std::string message_;
    UiControl last_key_ = UiControl::None;
    double last_key_time_ = 0.0;
    double mode_changed_at_ = 0.0;

    std::string& ActiveValue();
    const std::string& ActiveValue() const;
    void BeginEntry(double now_seconds);
};

}  // namespace clockin
