#pragma once

#include <cstddef>
#include <string>

namespace clockin {

constexpr std::size_t kMaxApiBaseUrlLength = 200;

// Validates an administrator-entered management server address and returns it
// without trailing slashes. Only http(s) origins with an optional path prefix
// are accepted; query strings, fragments, credentials and whitespace are not.
bool NormalizeApiBaseUrl(const std::string& input,
                         std::string& normalized,
                         std::string& error);

}  // namespace clockin
