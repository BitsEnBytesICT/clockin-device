#pragma once

#include <string>

namespace clockin {

class AdminPinStore {
public:
    bool Save(const std::string& path,
              const std::string& pin,
              std::string& error) const;
};

}  // namespace clockin
