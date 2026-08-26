#include "TestHarness.hpp"

#include "services/config/AdminPinStore.hpp"
#include "services/network/NetworkParsing.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace clockin;

TEST_CASE("Wi-Fi scan parsing merges duplicate radios and marks saved networks") {
    const std::string scans =
        "bssid / frequency / signal level / flags / ssid\n"
        "aa:00:00:00:00:01\t2412\t-72\t[WPA2-PSK-CCMP][ESS]\tOffice\n"
        "aa:00:00:00:00:02\t5180\t-42\t[WPA2-PSK-CCMP][ESS]\tOffice\n"
        "bb:00:00:00:00:01\t2437\t-55\t[ESS]\tGuest\n"
        "cc:00:00:00:00:01\t2462\t-31\t[WPA2-EAP-CCMP][ESS]\tCorporate\n";
    const std::string known =
        "network id / ssid / bssid / flags\n"
        "3\tOffice\tany\t[CURRENT]\n";
    const std::string status = "wpa_state=COMPLETED\nssid=Office\nid=3\n";
    const std::vector<WifiNetwork> networks = ParseWifiScan(scans, known, status);
    CHECK(networks.size() == 3);
    CHECK(networks[0].ssid == "Office");
    CHECK(networks[0].signal_dbm == -42);
    CHECK(networks[0].known);
    CHECK(networks[0].connected);
    const auto guest = std::find_if(networks.begin(), networks.end(), [](const auto& item) {
        return item.ssid == "Guest";
    });
    const auto corporate =
        std::find_if(networks.begin(), networks.end(), [](const auto& item) {
            return item.ssid == "Corporate";
        });
    CHECK(guest != networks.end());
    CHECK(guest->security == WifiSecurity::Open);
    CHECK(corporate != networks.end());
    CHECK(corporate->security == WifiSecurity::Unsupported);
}

TEST_CASE("Wi-Fi command encoding keeps SSIDs and passphrases inside one value") {
    CHECK(EncodeWpaHex("A B") == "412042");
    CHECK(QuoteWpaValue("a\"b\\c") == "\"a\\\"b\\\\c\"");
    CHECK(IsValidWifiPassword("12345678"));
    CHECK(IsValidWifiPassword(std::string(64, 'a')));
    CHECK(IsRawWifiPsk(std::string(64, 'a')));
    CHECK(!IsValidWifiPassword(std::string(64, 'z')));
    CHECK(!IsValidWifiPassword("short"));
    CHECK(!IsValidWifiPassword("line\nbreak"));
    CHECK(DecodeWpaText("Cafe\\x20West") == "Cafe West");
}

TEST_CASE("administrator PIN store replaces a protected file atomically") {
    char directory_template[] = "/tmp/clockin-pin-test-XXXXXX";
    char* directory = mkdtemp(directory_template);
    CHECK(directory != nullptr);
    const std::string path = std::string(directory) + "/admin-pin";
    AdminPinStore store;
    std::string error;
    CHECK(store.Save(path, "9071", error));
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    CHECK(value == "9071");
    struct stat metadata {};
    CHECK(stat(path.c_str(), &metadata) == 0);
    CHECK((metadata.st_mode & 0777) == 0600);
    CHECK(!store.Save(path, "12x4", error));
    unlink(path.c_str());
    rmdir(directory);
}
