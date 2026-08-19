#pragma once

#include <string>
#include <vector>

namespace clockin {

struct AppConfiguration {
    std::string api_base_url;
    std::string api_key;
    std::string admin_pin;
    std::string touch_device;
    std::string framebuffer_format;
    bool performance_overlay = false;

    bool ApiReady() const;
    bool AdminEnabled() const;

    static AppConfiguration Load(std::vector<std::string>* warnings = nullptr);
};

}  // namespace clockin
