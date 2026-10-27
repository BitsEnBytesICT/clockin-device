#include "TestHarness.hpp"

#include "domain/ApiUrl.hpp"

#include "app/Navigation.hpp"
#include "domain/AttendanceDate.hpp"
#include "domain/CardPresenceTracker.hpp"
#include "domain/RfidUid.hpp"

using namespace clockin;

TEST_CASE("attendance dates display only a validated calendar date") {
    CHECK(FormatAttendanceDate("2026-08-18") == "2026-08-18");
    CHECK(FormatAttendanceDate("2026-08-18T00:00:00.000Z") == "2026-08-18");
    CHECK(FormatAttendanceDate("2026-08-18T:00:00.000Z") == "2026-08-18");
    CHECK(FormatAttendanceDate("2024-02-29 12:30:00") == "2024-02-29");
    CHECK(FormatAttendanceDate("2026-02-29T00:00:00Z") ==
          "2026-02-29T00:00:00Z");
    CHECK(FormatAttendanceDate("onbekende datum") == "onbekende datum");
}

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

TEST_CASE("API base URLs accept http(s) origins and reject unsafe input") {
    std::string normalized;
    std::string error;
    CHECK(clockin::NormalizeApiBaseUrl("https://management-acc.bitsenbytes.net/",
                                       normalized, error));
    CHECK(normalized == "https://management-acc.bitsenbytes.net");
    CHECK(clockin::NormalizeApiBaseUrl("http://10.10.1.5:3000/clock", normalized, error));
    CHECK(normalized == "http://10.10.1.5:3000/clock");
    CHECK(!clockin::NormalizeApiBaseUrl("", normalized, error));
    CHECK(!error.empty());
    CHECK(!clockin::NormalizeApiBaseUrl("ftp://server", normalized, error));
    CHECK(!clockin::NormalizeApiBaseUrl("https://", normalized, error));
    CHECK(!clockin::NormalizeApiBaseUrl("https://user@host", normalized, error));
    CHECK(!clockin::NormalizeApiBaseUrl("https://host name", normalized, error));
    CHECK(!clockin::NormalizeApiBaseUrl("https://host/api?x=1", normalized, error));
    CHECK(!clockin::NormalizeApiBaseUrl("https://" + std::string(250, 'a'), normalized, error));
    CHECK(normalized.empty());
}
