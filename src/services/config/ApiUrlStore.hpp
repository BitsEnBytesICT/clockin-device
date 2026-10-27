#pragma once

#include <string>

namespace clockin {

// Persists the management server address chosen on the administrator screen.
// The file overrides BITS_BYTES_API_BASE_URL from the systemd environment.
class ApiUrlStore {
public:
    bool Save(const std::string& path,
              const std::string& url,
              std::string& normalized,
              std::string& error) const;
};

}  // namespace clockin
