#include "app/ApplicationController.hpp"

#include "services/hardware/IHardwareService.hpp"
#include "services/network/INetworkService.hpp"

#include <algorithm>

namespace clockin {

void ApplicationController::HandleAdminAction(AdminAction action,
                                              double now_seconds) {
    bool accepted = true;
    bool play_action_feedback = false;
    FeedbackCue action_feedback = FeedbackCue::Tap;
    switch (action.type) {
        case AdminActionType::None: break;
        case AdminActionType::Exit:
            workflow_.ReturnToWaiting(now_seconds, *this);
            break;
        case AdminActionType::ScanNetworks:
            accepted = network_.RequestScan();
            play_action_feedback = true;
            break;
        case AdminActionType::ConnectKnownNetwork:
            accepted = network_.ConnectKnown(action.network_id, action.ssid);
            play_action_feedback = true;
            break;
        case AdminActionType::ConnectNewNetwork:
            accepted = network_.ConnectNew(action.ssid, action.password,
                                           action.security, action.supports_sae);
            play_action_feedback = true;
            break;
        case AdminActionType::DisconnectNetwork:
            accepted = network_.Disconnect();
            play_action_feedback = true;
            action_feedback = FeedbackCue::Warning;
            break;
        case AdminActionType::ForgetNetwork:
            accepted = network_.Forget(action.network_id, action.ssid);
            play_action_feedback = true;
            action_feedback = FeedbackCue::Warning;
            break;
        case AdminActionType::SavePin: {
            std::string error;
            const bool saved = configuration_.admin_pin_persistable &&
                pin_store_.Save(configuration_.admin_pin_path, action.pin, error);
            if (saved) configuration_.admin_pin = action.pin;
            if (!configuration_.admin_pin_persistable) {
                error = "De vaste pincodeconfiguratie kan niet via het scherm worden gewijzigd.";
            }
            admin_page_.CompletePinSave(saved, error, now_seconds);
            hardware_.PlayFeedback(saved ? FeedbackCue::Success
                                         : FeedbackCue::Error);
            break;
        }
    }
    std::fill(action.password.begin(), action.password.end(), '\0');
    std::fill(action.pin.begin(), action.pin.end(), '\0');
    if (!accepted) {
        hardware_.PlayFeedback(FeedbackCue::Error);
        admin_page_.ShowNetworkError(
            "De netwerkopdracht kon niet worden gestart. Probeer opnieuw.");
    } else if (play_action_feedback) {
        hardware_.PlayFeedback(action_feedback);
    }
}

}  // namespace clockin
