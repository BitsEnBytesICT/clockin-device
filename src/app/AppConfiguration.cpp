#include "app/AppConfiguration.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace clockin {
namespace {

std::string Trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Environment(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : Trim(value);
}

std::string ReadSecret(const char* value_name,
                       const char* path_name,
                       const char* default_path) {
    std::string value = Environment(value_name);
    if (!value.empty()) return value;

    std::string path = Environment(path_name);
    if (path.empty()) path = default_path;
    std::ifstream input(path.c_str());
    if (!input.is_open()) return {};
    std::getline(input, value);
    return Trim(value);
}

bool IsFourDigitPin(const std::string& value) {
    return value.size() == 4 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

void Warn(std::vector<std::string>* warnings, const std::string& warning) {
    if (warnings != nullptr) warnings->push_back(warning);
}

}  // namespace

bool AppConfiguration::ApiReady() const {
    return !api_base_url.empty() && !api_key.empty();
}

bool AppConfiguration::AdminEnabled() const {
    return IsFourDigitPin(admin_pin);
}

AppConfiguration AppConfiguration::Load(std::vector<std::string>* warnings) {
    AppConfiguration config;
    config.api_base_url = Environment("BITS_BYTES_API_BASE_URL");
    while (!config.api_base_url.empty() && config.api_base_url.back() == '/') {
        config.api_base_url.pop_back();
    }
    config.api_key = ReadSecret("BITS_BYTES_API_KEY",
                                "BITS_BYTES_API_KEY_FILE",
                                "/etc/clockin-device/rfid-api-key");
    const std::string inline_admin_pin = Environment("BITS_BYTES_ADMIN_PIN");
    config.admin_pin_path = Environment("BITS_BYTES_ADMIN_PIN_FILE");
    if (config.admin_pin_path.empty()) {
        config.admin_pin_path = "/etc/clockin-device/admin-pin";
    }
    config.admin_pin_persistable = inline_admin_pin.empty();
    config.admin_pin = inline_admin_pin.empty()
                           ? ReadSecret("BITS_BYTES_ADMIN_PIN",
                                        "BITS_BYTES_ADMIN_PIN_FILE",
                                        "/etc/clockin-device/admin-pin")
                           : inline_admin_pin;
    config.touch_device = Environment("BITS_BYTES_TOUCH_DEVICE");
    if (config.touch_device.empty()) config.touch_device = "/dev/input/event1";
    config.framebuffer_format = Environment("BITS_BYTES_FRAMEBUFFER_FORMAT");
    if (config.framebuffer_format.empty()) config.framebuffer_format = "auto";
    if (config.framebuffer_format != "auto" &&
        config.framebuffer_format != "rgb565" &&
        config.framebuffer_format != "rgba8888") {
        Warn(warnings, "Invalid framebuffer format; using auto");
        config.framebuffer_format = "auto";
    }
    config.performance_overlay = Environment("BITS_BYTES_PERF_OVERLAY") == "1";

    if (config.api_base_url.empty()) Warn(warnings, "API base URL is not configured");
    if (config.api_key.empty()) Warn(warnings, "API key is not configured");
    if (!config.AdminEnabled()) Warn(warnings, "Admin PIN is missing or is not four digits");
    return config;
}

}  // namespace clockin
