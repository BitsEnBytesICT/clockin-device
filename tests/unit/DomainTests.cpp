#include "TestHarness.hpp"

#include "app/Navigation.hpp"
#include "domain/CardPresenceTracker.hpp"
#include "domain/RfidUid.hpp"

using namespace clockin;

TEST_CASE("navigation records every page transition") {
    Navigation navigation;
    CHECK(navigation.Current() == PageId::Waiting);
    const PageId pages[] = {PageId::Processing, PageId::Attendance, PageId::Signature,
                            PageId::Success, PageId::Error, PageId::AdminPin,
                            PageId::Admin, PageId::Waiting};
    double now = 1.0;
    for (PageId page : pages) {
        CHECK(navigation.Change(page, now));
        CHECK(navigation.Current() == page);
        CHECK_NEAR(navigation.EnteredAt(), now, 0.0001);
        CHECK(!navigation.Change(page, now + 0.5));
        now += 1.0;
    }
}

TEST_CASE("card presence emits once until removal") {
    CardPresenceTracker tracker(1.5);
    CHECK(tracker.Observe("11F3EF12", 1.0));
    CHECK(!tracker.Observe("11F3EF12", 1.5));
    CHECK(!tracker.Update(2.9));
    CHECK(tracker.Update(3.0));
    CHECK(!tracker.IsPresent());
    CHECK(tracker.Observe("11F3EF12", 3.1));
}

TEST_CASE("RFID UIDs are whitespace-free uppercase values") {
    CHECK(NormalizeRfidUid(" 11 f3\nef12\t") == "11F3EF12");
}
