#include "TestHarness.hpp"

#include "app/WorkflowCoordinator.hpp"
#include "fakes/FakeApiService.hpp"
#include "fakes/FakeHardwareService.hpp"
#include "fakes/RecordingWorkflowView.hpp"

namespace {

clockin::ApiResult Result(std::uint64_t id,
                          clockin::ApiJobType type,
                          clockin::ApiOutcome outcome) {
    clockin::ApiResult result;
    result.id = id;
    result.type = type;
    result.outcome = outcome;
    result.http_code = outcome == clockin::ApiOutcome::TransportFailed ? 0 : 200;
    result.message = outcome == clockin::ApiOutcome::Ok ? "ok" : "failure";
    return result;
}

}  // namespace

TEST_CASE("clock-in workflow reaches attendance, signature and success") {
    clockin::test::FakeApiService api;
    clockin::test::FakeHardwareService hardware;
    clockin::test::RecordingWorkflowView view;
    clockin::WorkflowCoordinator workflow(api, hardware);
    workflow.Start(0.0);
    hardware.Present(" 11f3 ef12 ");
    workflow.Update(clockin::PageId::Waiting, 1.0, view);
    CHECK(view.page == clockin::PageId::Processing);
    CHECK(api.jobs.back().rfid_uid == "11F3EF12");
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::CardAccepted) == 1);

    clockin::ApiResult scan =
        Result(api.LastId(clockin::ApiJobType::Scan), clockin::ApiJobType::Scan,
               clockin::ApiOutcome::Ok);
    scan.scan.action = "clock_in";
    scan.scan.user_name = "Ada Lovelace";
    api.Push(scan);
    workflow.Update(clockin::PageId::Processing, 1.1, view);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::Success) == 0);
    CHECK(api.Count(clockin::ApiJobType::Attendance) == 1);

    clockin::ApiResult attendance =
        Result(api.LastId(clockin::ApiJobType::Attendance),
               clockin::ApiJobType::Attendance, clockin::ApiOutcome::Ok);
    attendance.attendance_dates = {"2026-08-18", "2026-08-19"};
    api.Push(attendance);
    workflow.Update(clockin::PageId::Processing, 1.2, view);
    CHECK(view.page == clockin::PageId::Attendance);
    CHECK(view.dates.size() == 2);

    CHECK(workflow.SubmitSignature("<svg/>", 1.3, view));
    CHECK(view.page == clockin::PageId::Processing);
    api.Push(Result(api.LastId(clockin::ApiJobType::Signature),
                    clockin::ApiJobType::Signature, clockin::ApiOutcome::Ok));
    workflow.Update(clockin::PageId::Processing, 1.4, view);
    CHECK(view.page == clockin::PageId::Success);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::Success) == 1);
}

TEST_CASE("health refresh requests coalesce while a health check is pending") {
    clockin::test::FakeApiService api;
    clockin::test::FakeHardwareService hardware;
    clockin::test::RecordingWorkflowView view;
    clockin::WorkflowCoordinator workflow(api, hardware);
    workflow.Start(0.0);
    CHECK(api.Count(clockin::ApiJobType::Health) == 1);
    workflow.RequestHealth(0.1);
    workflow.RequestHealth(0.2);
    CHECK(api.Count(clockin::ApiJobType::Health) == 1);
    api.Push(Result(api.LastId(clockin::ApiJobType::Health),
                    clockin::ApiJobType::Health, clockin::ApiOutcome::Ok));
    workflow.Update(clockin::PageId::Admin, 0.3, view);
    CHECK(api.Count(clockin::ApiJobType::Health) == 2);
}

TEST_CASE("held card cannot create a second clock-out") {
    clockin::test::FakeApiService api;
    clockin::test::FakeHardwareService hardware;
    clockin::test::RecordingWorkflowView view;
    clockin::WorkflowCoordinator workflow(api, hardware);
    workflow.Start(0.0);
    hardware.Present("AABBCCDD");
    workflow.Update(clockin::PageId::Waiting, 1.0, view);
    clockin::ApiResult scan =
        Result(api.LastId(clockin::ApiJobType::Scan), clockin::ApiJobType::Scan,
               clockin::ApiOutcome::Ok);
    scan.scan.action = "clock_out";
    scan.scan.user_name = "Grace Hopper";
    api.Push(scan);
    workflow.Update(clockin::PageId::Processing, 1.1, view);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::Success) == 1);
    workflow.ReturnToWaiting(1.2, view);
    hardware.Present("AABBCCDD");
    workflow.Update(clockin::PageId::Waiting, 1.3, view);
    CHECK(api.Count(clockin::ApiJobType::Scan) == 1);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::CardAccepted) == 1);
    workflow.Update(clockin::PageId::Waiting, 3.0, view);
    hardware.Present("AABBCCDD");
    workflow.Update(clockin::PageId::Waiting, 3.1, view);
    CHECK(api.Count(clockin::ApiJobType::Scan) == 2);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::CardAccepted) == 2);
}

TEST_CASE("administrator RFID feedback sounds once per card presentation") {
    clockin::test::FakeApiService api;
    clockin::test::FakeHardwareService hardware;
    clockin::test::RecordingWorkflowView view;
    clockin::WorkflowCoordinator workflow(api, hardware);
    workflow.Start(0.0);

    hardware.Present("A1B2C3D4");
    workflow.Update(clockin::PageId::Admin, 1.0, view);
    hardware.Present("A1B2C3D4");
    workflow.Update(clockin::PageId::Admin, 1.1, view);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::CardAccepted) == 1);

    workflow.Update(clockin::PageId::Admin, 3.0, view);
    hardware.Present("A1B2C3D4");
    workflow.Update(clockin::PageId::Admin, 3.1, view);
    CHECK(hardware.FeedbackCount(clockin::FeedbackCue::CardAccepted) == 2);
}

TEST_CASE("API rejection, cancellation, timeout and uncertain result show errors") {
    const clockin::ApiOutcome outcomes[] = {
        clockin::ApiOutcome::Rejected, clockin::ApiOutcome::Cancelled,
        clockin::ApiOutcome::TransportFailed, clockin::ApiOutcome::Uncertain};
    for (clockin::ApiOutcome outcome : outcomes) {
        clockin::test::FakeApiService api;
        clockin::test::FakeHardwareService hardware;
        clockin::test::RecordingWorkflowView view;
        clockin::WorkflowCoordinator workflow(api, hardware);
        workflow.Start(0.0);
        hardware.Present("01020304");
        workflow.Update(clockin::PageId::Waiting, 1.0, view);
        api.Push(Result(api.LastId(clockin::ApiJobType::Scan),
                        clockin::ApiJobType::Scan, outcome));
        workflow.Update(clockin::PageId::Processing, 1.1, view);
        CHECK(view.page == clockin::PageId::Error);
        CHECK(hardware.FeedbackCount(clockin::FeedbackCue::Error) == 1);
        if (outcome == clockin::ApiOutcome::Uncertain) CHECK(!view.detail.empty());
    }
}

TEST_CASE("changing the API address re-checks health against the new server") {
    clockin::test::FakeApiService api;
    clockin::test::FakeHardwareService hardware;
    clockin::test::RecordingWorkflowView view;
    clockin::WorkflowCoordinator workflow(api, hardware);
    workflow.Start(0.0);
    const std::uint64_t old_health = api.LastId(clockin::ApiJobType::Health);
    workflow.ChangeApiBaseUrl("https://new.example", 1.0);
    CHECK(api.base_url == "https://new.example");
    CHECK(!workflow.Session().health_known);

    // The answer from the old server must not be shown for the new address.
    api.Push(Result(old_health, clockin::ApiJobType::Health, clockin::ApiOutcome::Ok));
    workflow.Update(clockin::PageId::Admin, 1.1, view);
    CHECK(!workflow.Session().health_known);
    CHECK(api.Count(clockin::ApiJobType::Health) == 2);

    api.Push(Result(api.LastId(clockin::ApiJobType::Health), clockin::ApiJobType::Health,
                    clockin::ApiOutcome::TransportFailed));
    workflow.Update(clockin::PageId::Admin, 1.2, view);
    CHECK(workflow.Session().health_known);
    CHECK(!workflow.Session().health_online);
}
