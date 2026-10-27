#include "TestHarness.hpp"

#include "app/AppConfiguration.hpp"
#include "services/config/ApiUrlStore.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

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
    ScopedEnvironment url_file("BITS_BYTES_API_BASE_URL_FILE", "/missing/clockin-url");
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
    ScopedEnvironment url_file("BITS_BYTES_API_BASE_URL_FILE", "/missing/clockin-url");
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

TEST_CASE("an address saved on the device overrides the environment") {
    char directory_template[] = "/tmp/clockin-config-XXXXXX";
    const char* directory = mkdtemp(directory_template);
    CHECK(directory != nullptr);
    const std::string path = std::string(directory) + "/api-base-url";
    ScopedEnvironment url("BITS_BYTES_API_BASE_URL", "https://from-environment.example");
    ScopedEnvironment url_file("BITS_BYTES_API_BASE_URL_FILE", path.c_str());
    ScopedEnvironment key("BITS_BYTES_API_KEY", "external-key");
    ScopedEnvironment pin("BITS_BYTES_ADMIN_PIN", "1234");

    CHECK(clockin::AppConfiguration::Load().api_base_url ==
          "https://from-environment.example");

    std::string normalized;
    std::string error;
    CHECK(clockin::ApiUrlStore().Save(path, "https://saved.example/", normalized, error));
    CHECK(normalized == "https://saved.example");
    CHECK(clockin::AppConfiguration::Load().api_base_url == "https://saved.example");
    CHECK(!clockin::ApiUrlStore().Save(path, "not a url", normalized, error));
    CHECK(clockin::AppConfiguration::Load().api_base_url == "https://saved.example");

    {
        std::ofstream corrupt(path.c_str());
        corrupt << "ftp://bad\n";
    }
    std::vector<std::string> warnings;
    CHECK(clockin::AppConfiguration::Load(&warnings).api_base_url ==
          "https://from-environment.example");
    CHECK(!warnings.empty());
    std::remove(path.c_str());
    rmdir(directory);
}
