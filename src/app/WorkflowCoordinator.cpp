#include "app/WorkflowCoordinator.hpp"

#include "app/WorkflowView.hpp"
#include "domain/RfidUid.hpp"
#include "services/api/IApiService.hpp"
#include "services/hardware/IHardwareService.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace clockin {

WorkflowCoordinator::WorkflowCoordinator(IApiService& api, IHardwareService& hardware)
    : api_(api), hardware_(hardware), card_presence_(1.5) {}

void WorkflowCoordinator::Start(double now_seconds) {
    if (api_.Start()) {
        api_.SubmitHealth();
    } else {
        std::fprintf(stderr, "API worker could not start; UI remains available offline\n");
        session_.health_known = true;
        session_.health_online = false;
    }
    session_.next_health_check = now_seconds + 15.0;
}

void WorkflowCoordinator::Stop() {
    api_.Stop();
    hardware_.Stop();
}

void WorkflowCoordinator::Update(PageId page,
                                 double now_seconds,
                                 IWorkflowView& view) {
    hardware_.Update(now_seconds);
    DrainApi(now_seconds, view);
    DrainRfid(page, now_seconds, view);
    if (now_seconds >= session_.next_health_check && !session_.ApiBusy() &&
        page == PageId::Waiting) {
        api_.SubmitHealth();
        session_.next_health_check = now_seconds + 15.0;
    }
}

bool WorkflowCoordinator::ConsumeActivity() {
    const bool detected = activity_detected_;
    activity_detected_ = false;
    return detected;
}

bool WorkflowCoordinator::SubmitSignature(const std::string& svg,
                                          double now_seconds,
                                          IWorkflowView& view) {
    return Submit(WorkflowRequest::Signature,
                  api_.SubmitSignature(session_.pending_rfid_uid, svg),
                  "Handtekening versturen...", now_seconds, view);
}

void WorkflowCoordinator::ReturnToWaiting(double now_seconds, IWorkflowView& view) {
    const bool keep_removal = card_presence_.IsPresent();
    const std::string present_uid = card_presence_.Uid();
    session_.ResetWorkflow();
    if (keep_removal) RequireCardRemoval(present_uid);
    else {
        session_.require_card_removal = false;
        session_.removal_uid.clear();
    }
    hardware_.CancelFeedback();
    view.ShowWaiting(now_seconds);
}

void WorkflowCoordinator::HandleApiResult(const ApiResult& result,
                                          double now_seconds,
                                          IWorkflowView& view) {
    if (result.type == ApiJobType::Health) {
        session_.health_known = true;
        session_.health_online = result.IsOk();
        session_.next_health_check = now_seconds + (result.IsOk() ? 60.0 : 15.0);
        std::printf("API %s: %s%s\n", result.IsOk() ? "reachable" : "unavailable",
                    api_.BaseUrl().c_str(), api_.HasApiKey() ? "" : " (API key missing)");
        return;
    }
    if (!IsCurrentApiResult(session_.request_id, result)) return;
    session_.health_known = true;
    session_.health_online = result.http_code > 0;
    session_.next_health_check = now_seconds + (session_.health_online ? 60.0 : 15.0);
    const WorkflowRequest completed = session_.request;
    session_.request_id = 0;
    session_.request = WorkflowRequest::None;
    if (!result.IsOk()) {
        if (completed == WorkflowRequest::Attendance &&
            result.outcome != ApiOutcome::Cancelled) {
            view.ShowAttendance(session_.user_name, {}, "Aanwezigheid niet beschikbaar",
                                now_seconds);
            return;
        }
        Error(result.message, result.outcome == ApiOutcome::Uncertain, now_seconds, view);
        return;
    }
    if (completed == WorkflowRequest::Scan) {
        session_.user_name = result.scan.user_name;
        session_.user_department = result.scan.user_department;
        session_.action = result.scan.action;
        std::printf("%s - %s\n", session_.action.c_str(), session_.user_name.c_str());
        if (session_.action == "clock_out") {
            hardware_.Beep();
            RequireCardRemoval(session_.pending_rfid_uid);
            view.ShowSuccess(session_.user_name, session_.action, now_seconds);
        } else {
            hardware_.Buzz();
            Submit(WorkflowRequest::Attendance,
                   api_.SubmitAttendance(session_.pending_rfid_uid),
                   "Aanwezigheid laden...", now_seconds, view);
        }
    } else if (completed == WorkflowRequest::Attendance) {
        view.ShowAttendance(session_.user_name, result.attendance_dates, {}, now_seconds);
    } else if (completed == WorkflowRequest::Signature) {
        hardware_.Buzz();
        RequireCardRemoval(session_.pending_rfid_uid);
        view.ShowSuccess(session_.user_name, session_.action, now_seconds);
    }
}

void WorkflowCoordinator::DrainApi(double now_seconds, IWorkflowView& view) {
    ApiResult result;
    while (api_.TryPop(result)) HandleApiResult(result, now_seconds, view);
}

void WorkflowCoordinator::DrainRfid(PageId page,
                                    double now_seconds,
                                    IWorkflowView& view) {
    RfidCard card;
    while (hardware_.PopCard(card)) {
        if (!card.valid) continue;
        activity_detected_ = true;
        std::string uid = NormalizeRfidUid(card.uid);
        if (uid.empty() || uid.size() > 64) continue;
        const bool new_presentation = card_presence_.Observe(uid, now_seconds);
        if (page == PageId::Admin) {
            view.ShowAdminCard(uid);
            RequireCardRemoval(uid);
            continue;
        }
        if (page != PageId::Waiting || session_.ApiBusy() || !new_presentation) continue;
        if (session_.require_card_removal) {
            if (uid == session_.removal_uid) continue;
            session_.require_card_removal = false;
            session_.removal_uid.clear();
        }
        session_.pending_rfid_uid = uid;
        RequireCardRemoval(uid);
        std::printf("RFID card: %s\n", uid.c_str());
        Submit(WorkflowRequest::Scan, api_.SubmitScan(uid), "Kaart verwerken...",
               now_seconds, view);
    }
    if (card_presence_.Update(now_seconds) && session_.require_card_removal) {
        session_.require_card_removal = false;
        session_.removal_uid.clear();
    }
}

bool WorkflowCoordinator::Submit(WorkflowRequest request,
                                 std::uint64_t request_id,
                                 const std::string& message,
                                 double now_seconds,
                                 IWorkflowView& view) {
    if (request_id == 0) {
        Error("Verzoek kon niet worden gestart", false, now_seconds, view);
        return false;
    }
    session_.request = request;
    session_.request_id = request_id;
    view.ShowProcessing(message, now_seconds);
    return true;
}

void WorkflowCoordinator::Error(const std::string& message,
                                bool uncertain,
                                double now_seconds,
                                IWorkflowView& view) {
    const std::string resolved = message.empty() ? "Er is iets misgegaan" : message;
    const std::string detail = uncertain
                                   ? (card_presence_.IsPresent()
                                          ? "Verwijder de kaart en scan opnieuw"
                                          : "Scan de kaart opnieuw")
                                   : std::string();
    if (uncertain && card_presence_.IsPresent()) {
        RequireCardRemoval(session_.pending_rfid_uid);
    }
    view.ShowError(resolved, detail, now_seconds);
}

void WorkflowCoordinator::RequireCardRemoval(const std::string& uid) {
    session_.require_card_removal = true;
    session_.removal_uid = uid;
}

}  // namespace clockin
