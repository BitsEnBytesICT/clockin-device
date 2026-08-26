#include "TestHarness.hpp"

#include "app/AppConfiguration.hpp"

#include <cstdlib>

namespace {

class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const char* value) : name_(name) {
        const char* previous = std::getenv(name);
        if (previous != nullptr) {
            had_previous_ = true;
            previous_ = previous;
        }
        if (value == nullptr) unsetenv(name);
        else setenv(name, value, 1);
    }
    ~ScopedEnvironment() {
        if (had_previous_) setenv(name_.c_str(), previous_.c_str(), 1);
        else unsetenv(name_.c_str());
    }

private:
    std::string name_;
    std::string previous_;
    bool had_previous_ = false;
};

}  // namespace

TEST_CASE("configuration accepts externally supplied credentials") {
    ScopedEnvironment url("BITS_BYTES_API_BASE_URL", "http://127.0.0.1:3000/");
    ScopedEnvironment key("BITS_BYTES_API_KEY", "external-key");
    ScopedEnvironment pin("BITS_BYTES_ADMIN_PIN", "1234");
    ScopedEnvironment format("BITS_BYTES_FRAMEBUFFER_FORMAT", "rgb565");
    std::vector<std::string> warnings;
    const clockin::AppConfiguration config = clockin::AppConfiguration::Load(&warnings);
    CHECK(config.api_base_url == "http://127.0.0.1:3000");
    CHECK(config.api_key == "external-key");
    CHECK(config.admin_pin == "1234");
    CHECK(!config.admin_pin_persistable);
    CHECK(config.ApiReady());
    CHECK(config.AdminEnabled());
    CHECK(warnings.empty());
}

TEST_CASE("missing secrets disable protected operations without crashing") {
    ScopedEnvironment url("BITS_BYTES_API_BASE_URL", nullptr);
    ScopedEnvironment key("BITS_BYTES_API_KEY", nullptr);
    ScopedEnvironment key_file("BITS_BYTES_API_KEY_FILE", "/missing/clockin-key");
    ScopedEnvironment pin("BITS_BYTES_ADMIN_PIN", nullptr);
    ScopedEnvironment pin_file("BITS_BYTES_ADMIN_PIN_FILE", "/missing/clockin-pin");
    ScopedEnvironment format("BITS_BYTES_FRAMEBUFFER_FORMAT", "invalid");
    std::vector<std::string> warnings;
    const clockin::AppConfiguration config = clockin::AppConfiguration::Load(&warnings);
    CHECK(!config.ApiReady());
    CHECK(!config.AdminEnabled());
    CHECK(config.framebuffer_format == "auto");
    CHECK(warnings.size() >= 3);
}
