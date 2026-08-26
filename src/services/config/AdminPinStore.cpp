#include "services/config/AdminPinStore.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace clockin {
namespace {

bool ValidPin(const std::string& pin) {
    return pin.size() == 4 &&
           std::all_of(pin.begin(), pin.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

std::string ParentDirectory(const std::string& path) {
    const std::size_t separator = path.find_last_of('/');
    if (separator == std::string::npos) return ".";
    if (separator == 0) return "/";
    return path.substr(0, separator);
}

bool WriteAll(int descriptor, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t count = write(descriptor, data, size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        data += count;
        size -= static_cast<std::size_t>(count);
    }
    return true;
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
    std::string pattern = path + ".tmp.XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    const int descriptor = mkstemp(temporary.data());
    if (descriptor < 0) {
        error = std::string("Tijdelijk pincodebestand maken mislukt: ") +
                std::strerror(errno);
        return false;
    }
    const std::string contents = pin + "\n";
    bool succeeded = fchmod(descriptor, 0600) == 0 &&
                     WriteAll(descriptor, contents.data(), contents.size()) &&
                     fsync(descriptor) == 0;
    const int saved_errno = errno;
    close(descriptor);
    if (succeeded) {
        succeeded = rename(temporary.data(), path.c_str()) == 0;
    }
    if (succeeded) {
        const std::string directory = ParentDirectory(path);
        const int directory_descriptor = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
        if (directory_descriptor >= 0) {
            succeeded = fsync(directory_descriptor) == 0;
            close(directory_descriptor);
        } else {
            succeeded = false;
        }
    }
    if (!succeeded) {
        const int failure = errno == 0 ? saved_errno : errno;
        unlink(temporary.data());
        error = std::string("Pincode veilig opslaan mislukt: ") + std::strerror(failure);
        return false;
    }
    return true;
}

}  // namespace clockin
