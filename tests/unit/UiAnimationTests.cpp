#include "TestHarness.hpp"

#include "ui/components/ButtonFeedback.hpp"
#include "ui/components/PageTransition.hpp"

#include <algorithm>
#include <cmath>
#include "ui/components/StatusAnimation.hpp"

using namespace clockin;

TEST_CASE("success animation progresses smoothly to a stable final state") {
    const SuccessAnimationState before = EvaluateSuccessAnimation(-1.0);
    const SuccessAnimationState early = EvaluateSuccessAnimation(0.2);
    const SuccessAnimationState middle = EvaluateSuccessAnimation(0.45);
    const SuccessAnimationState complete = EvaluateSuccessAnimation(1.0);

    CHECK_NEAR(before.circle_scale, 0.82f, 0.0001f);
    CHECK_NEAR(before.circle_opacity, 0.0f, 0.0001f);
    CHECK_NEAR(before.check_progress, 0.0f, 0.0001f);
    CHECK(early.check_progress > before.check_progress);
    CHECK(middle.check_progress > early.check_progress);
    CHECK(complete.check_progress >= middle.check_progress);
    CHECK_NEAR(complete.circle_scale, 1.0f, 0.0001f);
    CHECK_NEAR(complete.circle_opacity, 1.0f, 0.0001f);
    CHECK_NEAR(complete.check_progress, 1.0f, 0.0001f);
    CHECK_NEAR(complete.halo_opacity, 0.0f, 0.0001f);
}

TEST_CASE("success checkmark uses one connected path with rounded endpoints") {
    const ImVec2 center{400.0f, 190.0f};
    const SuccessCheckmarkGeometry empty =
        BuildSuccessCheckmarkGeometry(center, 1.0f, 0.0f);
    const SuccessCheckmarkGeometry partial =
        BuildSuccessCheckmarkGeometry(center, 1.0f, 0.2f);
    const SuccessCheckmarkGeometry complete =
        BuildSuccessCheckmarkGeometry(center, 1.0f, 1.0f);

    CHECK(empty.point_count == 0);
    CHECK(partial.point_count == 2);
    CHECK(complete.point_count == 3);
    CHECK_NEAR(complete.points[0].x, 376.0f, 0.0001f);
    CHECK_NEAR(complete.points[0].y, 190.0f, 0.0001f);
    CHECK_NEAR(complete.points[1].x, 394.0f, 0.0001f);
    CHECK_NEAR(complete.points[1].y, 208.0f, 0.0001f);
    CHECK_NEAR(complete.points[2].x, 431.0f, 0.0001f);
    CHECK_NEAR(complete.points[2].y, 165.0f, 0.0001f);
}

TEST_CASE("error animation draws both strokes and settles") {
    const ErrorAnimationState start = EvaluateErrorAnimation(0.0);
    const ErrorAnimationState middle = EvaluateErrorAnimation(0.3);
    const ErrorAnimationState complete = EvaluateErrorAnimation(1.0);

    CHECK_NEAR(start.circle_opacity, 0.0f, 0.0001f);
    CHECK_NEAR(start.first_stroke_progress, 0.0f, 0.0001f);
    CHECK(middle.first_stroke_progress > start.first_stroke_progress);
    CHECK(middle.second_stroke_progress > start.second_stroke_progress);
    CHECK_NEAR(complete.circle_scale, 1.0f, 0.0001f);
    CHECK_NEAR(complete.first_stroke_progress, 1.0f, 0.0001f);
    CHECK_NEAR(complete.second_stroke_progress, 1.0f, 0.0001f);
    CHECK_NEAR(complete.halo_opacity, 0.0f, 0.0001f);
}

TEST_CASE("page transition is brief monotonic and fully settles") {
    const PageTransitionState start = EvaluatePageTransition(0.0);
    const PageTransitionState middle = EvaluatePageTransition(0.11);
    const PageTransitionState complete = EvaluatePageTransition(0.22);

    CHECK_NEAR(start.opacity, 0.35f, 0.0001f);
    CHECK_NEAR(start.offset_y, 8.0f, 0.0001f);
    CHECK(middle.opacity > start.opacity);
    CHECK(middle.offset_y < start.offset_y);
    CHECK_NEAR(complete.opacity, 1.0f, 0.0001f);
    CHECK_NEAR(complete.offset_y, 0.0f, 0.0001f);
}

TEST_CASE("button press and release scale changes continuously between frames") {
    ButtonFeedback feedback;
    const UiControl button = UiControl::AttendanceConfirm;
    feedback.Press(button, 10.0);
    float previous = feedback.Scale(button, 10.0);
    CHECK_NEAR(previous, 1.0f, 0.0001f);
    // 1 ms steps: no step may jump by more than a fraction of a percent.
    for (int step = 1; step <= 120; ++step) {
        const float scale = feedback.Scale(button, 10.0 + step * 0.001);
        CHECK(std::fabs(scale - previous) < 0.002f);
        previous = scale;
    }
    CHECK(previous < 0.975f);
    feedback.Release(button, 10.12);
    float peak = 0.0f;
    for (int step = 0; step <= 260; ++step) {
        const float scale = feedback.Scale(button, 10.12 + step * 0.001);
        CHECK(std::fabs(scale - previous) < 0.002f);
        peak = std::max(peak, scale);
        previous = scale;
    }
    CHECK(peak > 1.01f);
    CHECK_NEAR(feedback.Scale(button, 11.0), 1.0f, 0.0001f);

    feedback.Press(button, 20.0);
    feedback.CancelPress(20.05);
    CHECK(!feedback.IsAnimating(button, 20.06));
    CHECK(feedback.Scale(button, 20.06) < 1.0f);
    CHECK_NEAR(feedback.Scale(button, 20.4), 1.0f, 0.0001f);
}
