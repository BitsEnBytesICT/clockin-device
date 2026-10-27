#include "TestHarness.hpp"

#include "pages/AdminPage.hpp"
#include "pages/AdminPinPage.hpp"
#include "pages/AttendancePage.hpp"
#include "pages/ErrorPage.hpp"
#include "pages/ProcessingPage.hpp"
#include "pages/SignaturePage.hpp"
#include "pages/SuccessPage.hpp"
#include "pages/WaitingPage.hpp"
#include "pages/admin/AdminPinPanel.hpp"
#include "pages/admin/AdminNetworkPanel.hpp"
#include "pages/admin/OnScreenKeyboard.hpp"

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
    CHECK(attendance.HitTest({700, 204}) == UiControl::AttendanceBack);
    CHECK(attendance.HitTest({700, 276}) == UiControl::AttendanceConfirm);
    CHECK(signature.HitTest({700, 168}) == UiControl::SignatureClear);
    CHECK(signature.HitTest({700, 240}) == UiControl::SignatureCancel);
    CHECK(signature.HitTest({700, 312}) == UiControl::SignatureSubmit);
    CHECK(attendance.HitTest({700, 40}) == UiControl::None);
    CHECK(attendance.HitTest({700, 240}) == UiControl::None);
    CHECK(signature.HitTest({700, 40}) == UiControl::None);
    CHECK(signature.HitTest({700, 204}) == UiControl::None);
    CHECK(signature.HitTest({700, 276}) == UiControl::None);
    CHECK(signature.HitTest({700, 430}) == UiControl::None);
    CHECK(admin.HitTest({700, 40}) == UiControl::AdminBack);
    CHECK(pin.HitTest({700, 40}) == UiControl::AdminBack);
    CHECK(pin.HitTest({265, 210}) == UiControl::Key1);
    CHECK(pin.HitTest({400, 280}) == UiControl::Key5);
    CHECK(pin.HitTest({500, 350}) == UiControl::Key9);
    CHECK(pin.HitTest({400, 420}) == UiControl::Key0);
    CHECK(processing.HitTest({400, 240}) == UiControl::None);
    CHECK(success.HitTest({400, 240}) == UiControl::None);
    CHECK(error.HitTest({400, 240}) == UiControl::None);
}

TEST_CASE("PIN change keeps the first entry and retries only a mismatched confirmation") {
    AdminPinPanel panel;
    panel.Reset(true);
    panel.Activate({UiControl::AdminPinChange, 1.0, {}});
    for (int digit : {1, 2, 3, 4}) {
        panel.Activate({static_cast<UiControl>(static_cast<int>(UiControl::Key0) + digit),
                        1.0 + digit * 0.01, {}});
    }
    for (int digit : {1, 2, 3, 5}) {
        const AdminAction action = panel.Activate(
            {static_cast<UiControl>(static_cast<int>(UiControl::Key0) + digit),
             2.0 + digit * 0.01, {}});
        CHECK(action.type == AdminActionType::None);
    }
    AdminAction saved;
    for (int digit : {1, 2, 3, 4}) {
        saved = panel.Activate(
            {static_cast<UiControl>(static_cast<int>(UiControl::Key0) + digit),
             3.0 + digit * 0.01, {}});
    }
    CHECK(saved.type == AdminActionType::SavePin);
    CHECK(saved.pin == "1234");
}

TEST_CASE("touch keyboard supports shift symbols spaces visibility and deletion") {
    OnScreenKeyboard keyboard;
    CHECK(keyboard.HitTest({60, 300}) == UiControl::AdminKeyboardShift);
    CHECK(keyboard.HitTest({700, 300}) == UiControl::AdminKeyboardBackspace);
    CHECK(keyboard.HitTest({677, 145}) == UiControl::AdminKeyboardVisibility);
    CHECK(keyboard.HitTest({30, 300}) == UiControl::None);
    CHECK(keyboard.HitTest({770, 300}) == UiControl::None);
    keyboard.Activate({UiControl::AdminKeyboardShift, 1.0, {50, 300}});
    keyboard.Activate({UiControl::AdminKeyboardKey, 1.1, {60, 200}});
    keyboard.Activate({UiControl::AdminKeyboardSpace, 1.2, {300, 355}});
    keyboard.Activate({UiControl::AdminKeyboardSymbols, 1.3, {50, 355}});
    keyboard.Activate({UiControl::AdminKeyboardKey, 1.4, {60, 200}});
    CHECK(keyboard.Value() == "Q 1");
    keyboard.Activate({UiControl::AdminKeyboardBackspace, 1.5, {700, 300}});
    CHECK(keyboard.Value() == "Q ");
    keyboard.Clear();
    CHECK(keyboard.Value().empty());
}

TEST_CASE("network details require confirmation and back closes one layer at a time") {
    AdminNetworkPanel panel;
    NetworkSnapshot snapshot;
    snapshot.state = NetworkState::Connected;
    snapshot.associated = true;
    snapshot.current_ssid = "TestNetwork";
    snapshot.networks.push_back(
        {"TestNetwork", "00:11:22:33:44:55", -45, 7,
         WifiSecurity::Personal, false, true, true});
    panel.SetSnapshot(snapshot);
    panel.Activate({UiControl::AdminNetworkSelect, 1.0, {200, 280}});
    CHECK(panel.HasSubflow());
    panel.Activate({UiControl::AdminNetworkForget, 1.1, {200, 400}});
    panel.Back(1.2);
    CHECK(panel.HasSubflow());
    panel.Back(1.3);
    CHECK(!panel.HasSubflow());
}

TEST_CASE("network password keyboard exposes its focused layout only while open") {
    AdminNetworkPanel panel;
    NetworkSnapshot snapshot;
    snapshot.state = NetworkState::Disconnected;
    snapshot.networks.push_back(
        {"NewNetwork", "00:11:22:33:44:55", -55, -1,
         WifiSecurity::Personal, false, false, false});
    panel.SetSnapshot(snapshot);

    panel.Activate({UiControl::AdminNetworkSelect, 1.0, {200, 280}});
    CHECK(!panel.KeyboardOpen());
    panel.Activate({UiControl::AdminNetworkConnect, 1.1, {600, 400}});
    CHECK(panel.KeyboardOpen());

    panel.Back(1.2);
    CHECK(!panel.KeyboardOpen());
    CHECK(panel.HasSubflow());
}

TEST_CASE("administrator PIN page owns and bounds its entry state") {
    AdminPinPage pin;
    for (int digit : {1, 2, 3, 4, 5}) pin.AppendDigit(digit, digit * 0.1);
    CHECK(pin.Value() == "1234");
    pin.Reset();
    CHECK(pin.Value().empty());
    pin.AppendDigit(0, 1.0);
    CHECK(pin.Value() == "0");
    pin.ApplyKey(UiControl::KeyBackspace, 1.1);
    CHECK(pin.Value().empty());
}

TEST_CASE("administrator tabs expose PIN confirmation and network refresh actions") {
    AdminPage admin;
    admin.Reset(true, 0.0);
    CHECK(admin.HitTest({400, 37}) == UiControl::AdminTabNetwork);
    AdminAction network = admin.Activate({UiControl::AdminTabNetwork, 1.0, {400, 37}});
    CHECK(network.type == AdminActionType::ScanNetworks);
    // Panels are drawn 35 px higher than their own layout.
    CHECK(admin.HitTest({711, 145}) == UiControl::AdminNetworkRefresh);
    CHECK(admin.HitTest({711, 180}) == UiControl::None);

    CHECK(admin.HitTest({240, 37}) == UiControl::AdminTabPin);
    admin.Activate({UiControl::AdminTabPin, 2.0, {240, 37}});
    CHECK(admin.HitTest({400, 300}) == UiControl::AdminPinChange);
    admin.Activate({UiControl::AdminPinChange, 2.1, {400, 300}});
    for (int pass = 0; pass < 2; ++pass) {
        for (int digit = 1; digit <= 4; ++digit) {
            const UiControl key = static_cast<UiControl>(
                static_cast<int>(UiControl::Key1) + digit - 1);
            const AdminAction action = admin.Activate({key, 3.0 + pass + digit * 0.01, {}});
            if (pass == 1 && digit == 4) {
                CHECK(action.type == AdminActionType::SavePin);
                CHECK(action.pin == "1234");
            }
        }
    }
}

TEST_CASE("administrator API tab edits the server address with the touch keyboard") {
    AdminPage admin;
    admin.Reset(true, 0.0);
    admin.SetApiUrl("https://old.example");
    CHECK(admin.HitTest({80, 37}) == UiControl::AdminTabRfid);
    CHECK(admin.HitTest({560, 37}) == UiControl::AdminTabApi);
    admin.Activate({UiControl::AdminTabApi, 1.0, {560, 37}});
    CHECK(admin.HitTest({400, 393}) == UiControl::AdminApiEdit);
    admin.Activate({UiControl::AdminApiEdit, 1.1, {400, 393}});

    // The keyboard is open: tabs are locked and the address field has no eye toggle.
    CHECK(admin.HitTest({560, 37}) == UiControl::None);
    CHECK(admin.HitTest({677, 145}) == UiControl::None);
    CHECK(admin.HitTest({700, 300}) == UiControl::AdminKeyboardBackspace);
    // Remove "example" from the prefilled address, then type "nl".
    for (int count = 0; count < 7; ++count) {
        admin.Activate({UiControl::AdminKeyboardBackspace, 1.2 + count * 0.01, {700, 300}});
    }
    admin.Activate({UiControl::AdminKeyboardKey, 1.3, {509, 300}});
    admin.Activate({UiControl::AdminKeyboardKey, 1.4, {686, 250}});
    AdminAction saved = admin.Activate({UiControl::AdminKeyboardConnect, 1.5, {590, 428}});
    CHECK(saved.type == AdminActionType::SaveApiUrl);
    CHECK(saved.api_url == "https://old.nl");

    admin.CompleteApiUrlSave(false, "Schijf vol", 1.6);
    CHECK(admin.HitTest({700, 300}) == UiControl::AdminKeyboardBackspace);
    admin.CompleteApiUrlSave(true, {}, 1.7);
    CHECK(admin.HitTest({400, 393}) == UiControl::AdminApiEdit);
}

TEST_CASE("API address keyboard rejects an invalid address without saving") {
    AdminPage admin;
    admin.Reset(true, 0.0);
    admin.SetApiUrl("https://old.example");
    admin.Activate({UiControl::AdminTabApi, 1.0, {560, 37}});
    admin.Activate({UiControl::AdminApiEdit, 1.1, {400, 393}});
    admin.Activate({UiControl::AdminKeyboardSpace, 1.2, {450, 355}});
    const AdminAction rejected =
        admin.Activate({UiControl::AdminKeyboardConnect, 1.3, {590, 428}});
    CHECK(rejected.type == AdminActionType::None);
    admin.Activate({UiControl::AdminBack, 1.4, {700, 37}});
    CHECK(admin.HitTest({560, 37}) == UiControl::AdminTabApi);
    const AdminAction exit = admin.Activate({UiControl::AdminBack, 1.5, {700, 37}});
    CHECK(exit.type == AdminActionType::Exit);
}
