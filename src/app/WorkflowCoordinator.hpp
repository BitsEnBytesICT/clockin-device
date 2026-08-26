#pragma once

#include "app/WorkflowSession.hpp"
#include "domain/CardPresenceTracker.hpp"
#include "pages/Page.hpp"
#include "services/api/ApiTypes.hpp"

#include <string>

namespace clockin {

class IApiService;
class IHardwareService;
class IWorkflowView;

class WorkflowCoordinator {
public:
    WorkflowCoordinator(IApiService& api, IHardwareService& hardware);

    void Start(double now_seconds);
    void Stop();
    void Update(PageId page, double now_seconds, IWorkflowView& view);
    bool SubmitSignature(const std::string& svg,
                         double now_seconds,
                         IWorkflowView& view);
    void ReturnToWaiting(double now_seconds, IWorkflowView& view);
    void RequestHealth(double now_seconds);

    const WorkflowSession& Session() const { return session_; }
    WorkflowSession& Session() { return session_; }
    bool ConsumeActivity();

private:
    IApiService& api_;
    IHardwareService& hardware_;
    CardPresenceTracker card_presence_;
    WorkflowSession session_;
    bool activity_detected_ = false;

    void HandleApiResult(const ApiResult& result,
                         double now_seconds,
                         IWorkflowView& view);
    void DrainApi(double now_seconds, IWorkflowView& view);
    void DrainRfid(PageId page, double now_seconds, IWorkflowView& view);
    bool Submit(WorkflowRequest request,
                std::uint64_t request_id,
                const std::string& message,
                double now_seconds,
                IWorkflowView& view);
    void Error(const std::string& message,
               bool uncertain,
               double now_seconds,
               IWorkflowView& view);
    void RequireCardRemoval(const std::string& uid);
};

}  // namespace clockin
