#include "TestHarness.hpp"

#include "pages/AdminPage.hpp"
#include "pages/AdminPinPage.hpp"
#include "pages/AttendancePage.hpp"
#include "pages/ErrorPage.hpp"
#include "pages/ProcessingPage.hpp"
#include "pages/SignaturePage.hpp"
#include "pages/SuccessPage.hpp"
#include "pages/WaitingPage.hpp"

using namespace clockin;

TEST_CASE("page hit regions expose only their own actions") {
    WaitingPage waiting;
    AttendancePage attendance;
    SignaturePage signature;
    AdminPinPage pin;
    AdminPage admin;
    ProcessingPage processing;
    SuccessPage success;
    ErrorPage error;

    CHECK(waiting.HitTest({700, 40}) == UiControl::Admin);
    CHECK(attendance.HitTest({700, 40}) == UiControl::AttendanceBack);
    CHECK(attendance.HitTest({700, 430}) == UiControl::AttendanceConfirm);
    CHECK(signature.HitTest({700, 40}) == UiControl::SignatureClear);
    CHECK(signature.HitTest({700, 235}) == UiControl::SignatureCancel);
    CHECK(signature.HitTest({700, 430}) == UiControl::SignatureSubmit);
    CHECK(admin.HitTest({700, 40}) == UiControl::AdminBack);
    CHECK(pin.HitTest({700, 40}) == UiControl::AdminBack);
    CHECK(pin.HitTest({265, 230}) == UiControl::Key1);
    CHECK(pin.HitTest({380, 320}) == UiControl::Key5);
    CHECK(pin.HitTest({495, 410}) == UiControl::Key9);
    CHECK(processing.HitTest({400, 240}) == UiControl::None);
    CHECK(success.HitTest({400, 240}) == UiControl::None);
    CHECK(error.HitTest({400, 240}) == UiControl::None);
}

TEST_CASE("administrator PIN page owns and bounds its entry state") {
    AdminPinPage pin;
    for (int digit : {1, 2, 3, 4, 5}) pin.AppendDigit(digit, digit * 0.1);
    CHECK(pin.Value() == "1234");
    pin.Reset();
    CHECK(pin.Value().empty());
    pin.AppendDigit(0, 1.0);
    CHECK(pin.Value().empty());
}
