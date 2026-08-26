#pragma once

#include "app/AppEvent.hpp"
#include "pages/Page.hpp"
#include "pages/admin/AdminAction.hpp"
#include "pages/admin/AdminNetworkPanel.hpp"
#include "pages/admin/AdminPinPanel.hpp"
#include "pages/admin/AdminRfidPanel.hpp"
#include "services/network/NetworkTypes.hpp"

#include <string>

namespace clockin {

class AdminPage final : public Page {
public:
    PageId Id() const override { return PageId::Admin; }
    UiControl HitTest(const ImVec2& point) const override;
    bool BeginScroll(const ImVec2& point) override;
    void ScrollBy(float delta_y) override;
    void EndScroll() override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    AdminAction Activate(const AppEvent& event);
    void SetUid(std::string uid);
    void SetNetworkSnapshot(NetworkSnapshot snapshot);
    void SetApiStatus(bool known, bool online);
    void CompletePinSave(bool succeeded,
                         const std::string& message,
                         double now_seconds);
    void ShowNetworkError(std::string message);
    void Reset(bool pin_persistable, double now_seconds);
    void ClearSecrets();

private:
    enum class Tab { Rfid, Pin, Network };

    Tab tab_ = Tab::Rfid;
    AdminRfidPanel rfid_panel_;
    AdminPinPanel pin_panel_;
    AdminNetworkPanel network_panel_;
    double tab_changed_at_ = 0.0;

    void ChangeTab(Tab tab, double now_seconds);
    bool ActiveSubflow() const;
};

}  // namespace clockin
