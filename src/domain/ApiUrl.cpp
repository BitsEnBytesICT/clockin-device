#include "domain/ApiUrl.hpp"

#include <cctype>

namespace clockin {
namespace {

bool StartsWith(const std::string& value, const char* prefix) {
    return value.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

bool ValidHostCharacter(unsigned char character) {
    return std::isalnum(character) != 0 || character == '.' || character == '-' ||
           character == ':' || character == '[' || character == ']';
}

bool ValidPathCharacter(unsigned char character) {
    return std::isalnum(character) != 0 || character == '/' || character == '.' ||
           character == '-' || character == '_' || character == '~';
}

}  // namespace

bool NormalizeApiBaseUrl(const std::string& input,
                         std::string& normalized,
                         std::string& error) {
    normalized.clear();
    error.clear();
    std::string value = input;
    while (!value.empty() && value.back() == '/') value.pop_back();
    if (value.empty()) {
        error = "Voer een serveradres in.";
        return false;
    }
    if (value.size() > kMaxApiBaseUrlLength) {
        error = "Het adres is te lang.";
        return false;
    }
    std::size_t host_start = 0;
    if (StartsWith(value, "https://")) host_start = 8;
    else if (StartsWith(value, "http://")) host_start = 7;
    else {
        error = "Het adres moet beginnen met https:// of http://";
        return false;
    }
    const std::size_t path_start = value.find('/', host_start);
    const std::string host = value.substr(
        host_start, path_start == std::string::npos ? std::string::npos
                                                    : path_start - host_start);
    if (host.empty() || host.front() == '.' || host.front() == '-' ||
        host.front() == ':' || host.back() == '.' || host.back() == ':') {
        error = "Het adres bevat geen geldige servernaam.";
        return false;
    }
    for (unsigned char character : host) {
        if (!ValidHostCharacter(character)) {
            error = "De servernaam bevat ongeldige tekens.";
            return false;
        }
    }
    if (path_start != std::string::npos) {
        for (std::size_t index = path_start; index < value.size(); ++index) {
            if (!ValidPathCharacter(static_cast<unsigned char>(value[index]))) {
                error = "Gebruik alleen een serveradres, zonder ?, # of spaties.";
                return false;
            }
        }
    }
    normalized = value;
    return true;
}

}  // namespace clockin
