#include "services/config/AdminPinStore.hpp"

#include "services/config/SecureFile.hpp"

#include <algorithm>
#include <cctype>

namespace clockin {
namespace {

bool ValidPin(const std::string& pin) {
    return pin.size() == 4 &&
           std::all_of(pin.begin(), pin.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

}  // namespace

bool AdminPinStore::Save(const std::string& path,
                         const std::string& pin,
                         std::string& error) const {
    error.clear();
    if (path.empty() || !ValidPin(pin)) {
        error = "De pincode moet uit precies vier cijfers bestaan";
        return false;
    }
    std::string contents = pin + "\n";
    std::string failure;
    const bool saved = WriteSecureFile(path, contents, failure);
    std::fill(contents.begin(), contents.end(), '\0');
    if (!saved) error = "Pincode veilig opslaan mislukt: " + failure;
    return saved;
}

}  // namespace clockin
