#include "services/config/ApiUrlStore.hpp"

#include "domain/ApiUrl.hpp"
#include "services/config/SecureFile.hpp"

namespace clockin {

bool ApiUrlStore::Save(const std::string& path,
                       const std::string& url,
                       std::string& normalized,
                       std::string& error) const {
    if (!NormalizeApiBaseUrl(url, normalized, error)) return false;
    std::string failure;
    if (!WriteSecureFile(path, normalized + "\n", failure)) {
        error = "Serveradres opslaan mislukt: " + failure;
        return false;
    }
    return true;
}

}  // namespace clockin
