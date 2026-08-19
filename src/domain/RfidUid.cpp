#include "domain/RfidUid.hpp"

#include <algorithm>
#include <cctype>

namespace clockin {

std::string NormalizeRfidUid(std::string uid) {
    uid.erase(std::remove_if(uid.begin(), uid.end(), [](unsigned char character) {
                  return std::isspace(character) != 0;
              }),
              uid.end());
    std::transform(uid.begin(), uid.end(), uid.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return uid;
}

}  // namespace clockin
