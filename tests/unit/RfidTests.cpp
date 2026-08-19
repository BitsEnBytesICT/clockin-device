#include "TestHarness.hpp"

#include "services/hardware/rfid/Mfrc522.hpp"

#include <algorithm>
#include <vector>

namespace {

class CapturingSpi final : public clockin::SpiTransport {
public:
    bool Transfer(const std::uint8_t* transmit,
                  std::uint8_t* receive,
                  std::size_t length,
                  std::string&) override {
        last_transmit.assign(transmit, transmit + length);
        std::fill(receive, receive + length, 0);
        if (length >= 2) receive[1] = response;
        return true;
    }

    std::uint8_t response = 0x92;
    std::vector<std::uint8_t> last_transmit;
};

}  // namespace

TEST_CASE("MFRC522 register reads use correct SPI framing") {
    CapturingSpi transport;
    clockin::Mfrc522 reader(transport);
    std::uint8_t version = 0;
    std::string error;
    CHECK(reader.ReadVersion(version, error));
    CHECK(version == 0x92);
    CHECK(transport.last_transmit.size() == 2);
    CHECK(transport.last_transmit[0] == 0xEE);
    CHECK(transport.last_transmit[1] == 0x00);
}
