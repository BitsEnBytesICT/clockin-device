#include "app/InputRouter.hpp"

#include "pages/Page.hpp"
#include "pages/SignaturePage.hpp"
#include "ui/components/ButtonFeedback.hpp"

#include <cmath>

namespace clockin {

bool InputRouter::Process(const std::vector<TouchEvent>& events,
                          Page& active_page,
                          SignaturePage& signature_page,
                          ButtonFeedback& feedback,
                          double now_seconds,
                          const Activate& activate) {
    bool activity_detected = false;
    for (const TouchEvent& event : events) {
        activity_detected = true;
        const PageId page = active_page.Id();
        if (page == PageId::Processing || page == PageId::Success || page == PageId::Error) {
            continue;
        }
        const bool signature_event = page == PageId::Signature &&
            (signature_page.IsDrawing() ||
             (event.type == TouchEventType::Down &&
              signature_page.IsSignatureArea(event.position)));
        if (signature_event) {
            if (event.type == TouchEventType::Down) {
                Reset();
                feedback.CancelPress();
                signature_page.BeginStroke(event.position);
            } else if (event.type == TouchEventType::Move && signature_page.IsDrawing()) {
                signature_page.ContinueStroke(event.position);
            } else if (event.type == TouchEventType::Up && signature_page.IsDrawing()) {
                signature_page.EndStroke();
            } else if (event.type == TouchEventType::Cancel) {
                signature_page.CancelStroke();
            }
            continue;
        }
        if (event.type == TouchEventType::Down) {
            gesture_.active = true;
            gesture_.moved = false;
            gesture_.start = event.position;
            gesture_.last = event.position;
            gesture_.target = active_page.HitTest(event.position);
            feedback.Press(gesture_.target);
            active_page.BeginScroll(event.position);
        } else if (event.type == TouchEventType::Move && gesture_.active) {
            if (Distance(event.position, gesture_.start) > 10.0f) {
                gesture_.moved = true;
                feedback.CancelPress();
            }
            active_page.ScrollBy(event.position.y - gesture_.last.y);
            gesture_.last = event.position;
        } else if (event.type == TouchEventType::Up && gesture_.active) {
            const UiControl release_target = active_page.HitTest(event.position);
            const bool valid_click = !gesture_.moved && gesture_.target != UiControl::None &&
                                     release_target == gesture_.target;
            const UiControl activated = gesture_.target;
            if (valid_click) feedback.Release(activated, now_seconds);
            else feedback.CancelPress();
            Reset();
            active_page.EndScroll();
            if (valid_click) activate({activated, now_seconds, event.position});
        } else if (event.type == TouchEventType::Cancel) {
            Reset();
            feedback.CancelPress();
            active_page.EndScroll();
            signature_page.CancelStroke();
        }
    }
    return activity_detected;
}

void InputRouter::Reset() { gesture_ = Gesture{}; }

float InputRouter::Distance(const ImVec2& first, const ImVec2& second) {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace clockin
