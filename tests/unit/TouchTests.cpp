#include "TestHarness.hpp"

#include "services/input/TouchInput.hpp"

namespace {

void Feed(clockin::TouchInput& touch,
          unsigned short type,
          unsigned short code,
          int value,
          double now) {
    input_event event{};
    event.type = type;
    event.code = code;
    event.value = value;
    touch.FeedEventForTest(event, now);
}

}  // namespace

TEST_CASE("touch parser rotates raw STM32 coordinates into 800x480 space") {
    clockin::TouchInput touch("/not-opened-during-test");
    touch.SetAxisRangesForTest(0, 480, 0, 800);
    touch.ClearEventsForTest();
    Feed(touch, EV_ABS, ABS_MT_SLOT, 0, 1.0);
    Feed(touch, EV_ABS, ABS_MT_TRACKING_ID, 1, 1.0);
    Feed(touch, EV_ABS, ABS_MT_POSITION_X, 480, 1.0);
    Feed(touch, EV_ABS, ABS_MT_POSITION_Y, 800, 1.0);
    Feed(touch, EV_SYN, SYN_REPORT, 0, 1.0);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events().front().type == clockin::TouchEventType::Down);
    CHECK_NEAR(touch.Events().front().position.x, 800.0f, 0.1f);
    CHECK_NEAR(touch.Events().front().position.y, 0.0f, 0.1f);

    Feed(touch, EV_ABS, ABS_MT_TRACKING_ID, -1, 1.1);
    Feed(touch, EV_SYN, SYN_REPORT, 0, 1.1);
    CHECK(touch.Events().back().type == clockin::TouchEventType::Up);
}
