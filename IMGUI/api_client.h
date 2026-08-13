#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <string>
#include <curl/curl.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include "rfid_reader.h"  // Add this include

#define API_BASE_URL "https://management.bitsenbytes.net"
#define API_ATTENDANCE_PATH "/api/attendance_last_30"

struct ScanResponse {
    bool success;
    std::string action;
    std::string message;
    std::string user_name;
    std::string user_department;
};

class APIClient {
private:
    RFIDReader* rfid_reader;  // Add pointer to RFIDReader
    std::string base_url;
    std::string api_key;
    bool mock_api;

    static std::string Trim(const std::string& value) {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static std::string ReadFirstLine(const std::string& path) {
        if (path.empty()) return "";
        std::ifstream input(path.c_str());
        if (!input.is_open()) return "";
        std::string line;
        std::getline(input, line);
        return Trim(line);
    }

    static std::string ResolveBaseURL() {
        const char* configured = std::getenv("BITS_BYTES_API_BASE_URL");
        std::string value = configured != nullptr ? Trim(configured) : API_BASE_URL;
        while (!value.empty() && value[value.size() - 1] == '/') value.erase(value.size() - 1);
        return value.empty() ? API_BASE_URL : value;
    }

    static std::string ResolveAPIKey() {
        const char* configured = std::getenv("BITS_BYTES_API_KEY");
        if (configured != nullptr && !Trim(configured).empty()) return Trim(configured);

        const char* configured_path = std::getenv("BITS_BYTES_API_KEY_FILE");
        std::string path = configured_path != nullptr ? Trim(configured_path) : "/etc/bitsenbytes/rfid-api-key";
        return ReadFirstLine(path);
    }

    curl_slist* BuildJSONHeaders() const {
        curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!api_key.empty()) {
            const std::string authorization = "Authorization: " + api_key;
            headers = curl_slist_append(headers, authorization.c_str());
        }
        return headers;
    }

    void ConfigureCommonOptions(CURL* curl) const {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "bits-bytes-stm32-rfid/1.0");
    }

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    // Helper function to escape JSON strings
    std::string EscapeJSON(const std::string& input) {
        std::ostringstream escaped;
        for (char c : input) {
            switch (c) {
                case '"':  escaped << "\\\""; break;
                case '\\': escaped << "\\\\"; break;
                case '\b': escaped << "\\b"; break;
                case '\f': escaped << "\\f"; break;
                case '\n': escaped << "\\n"; break;
                case '\r': escaped << "\\r"; break;
                case '\t': escaped << "\\t"; break;
                default:
                    if (c < 32) {
                        // Escape control characters
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        escaped << buf;
                    } else {
                        escaped << c;
                    }
            }
        }
        return escaped.str();
    }
    
    std::string ParseJSON(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos += search.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        
        size_t end = pos;
        while (end < json.length() && json[end] != '"' && json[end] != ',' && json[end] != '}') end++;
        
        return json.substr(pos, end - pos);
    }

    std::vector<std::string> ParseStringArray(const std::string& json, const std::string& key) {
        std::vector<std::string> result;
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return result;

        pos = json.find('[', pos + search.length());
        if (pos == std::string::npos) return result;

        size_t end = json.find(']', pos + 1);
        if (end == std::string::npos) return result;

        bool in_string = false;
        std::string current;
        for (size_t i = pos + 1; i < end; ++i) {
            char c = json[i];
            if (!in_string) {
                if (c == '"') {
                    in_string = true;
                    current.clear();
                }
                continue;
            }

            if (c == '\\' && i + 1 < end) {
                char next = json[i + 1];
                switch (next) {
                    case '"': current.push_back('"'); break;
                    case '\\': current.push_back('\\'); break;
                    case '/': current.push_back('/'); break;
                    case 'b': current.push_back('\b'); break;
                    case 'f': current.push_back('\f'); break;
                    case 'n': current.push_back('\n'); break;
                    case 'r': current.push_back('\r'); break;
                    case 't': current.push_back('\t'); break;
                    default: current.push_back(next); break;
                }
                i++;
                continue;
            }

            if (c == '"') {
                in_string = false;
                result.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(c);
        }

        return result;
    }
    
    bool ParseBool(const std::string& json, const std::string& key) {
        std::string value = ParseJSON(json, key);
        return (value == "true" || value == "True");
    }

public:
    // Constructor that accepts RFIDReader pointer
    APIClient(RFIDReader* reader = nullptr)
        : rfid_reader(reader),
          base_url(ResolveBaseURL()),
          api_key(ResolveAPIKey()),
          mock_api(false) {
#ifdef DESKTOP_SIM
        const char* mock_value = std::getenv("STM32_SIM_MOCK_API");
        mock_api = mock_value != nullptr && std::string(mock_value) == "1";
#endif
    }

    bool IsMockMode() const { return mock_api; }
    bool HasAPIKey() const { return !api_key.empty(); }
    const std::string& GetBaseURL() const { return base_url; }
    
    // Set the RFID reader (if not set in constructor)
    void SetRFIDReader(RFIDReader* reader) {
        rfid_reader = reader;
    }
    
    static std::string clean_rfid_uid(std::string rfid_uid_with_spaces) {
        rfid_uid_with_spaces.erase(
            std::remove_if(rfid_uid_with_spaces.begin(), rfid_uid_with_spaces.end(), [](char c) {
                return std::isspace(static_cast<unsigned char>(c));
            }),
            rfid_uid_with_spaces.end()
        );
        return rfid_uid_with_spaces;
    }

    bool TestConnection() {
        if (mock_api) {
            printf("+ Mock API enabled (no network requests)\n");
            return true;
        }

        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        std::string response;
        const std::string url = base_url + "/api/health";
        curl_slist* headers = BuildJSONHeaders();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        ConfigureCommonOptions(curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        
        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && response_code == 200) {
            printf("+ API reachable: %s%s\n", base_url.c_str(), api_key.empty() ? " (no API key configured)" : " (Authorization configured)");
            return true;
        }
        
        printf("- API connection failed (code=%ld, curl=%s)\n", response_code, curl_easy_strerror(res));
        return false;
    }

    bool FetchAttendanceLast30Days(const std::string& raw_rfid_uid, std::vector<std::string>& out_dates) {
        out_dates.clear();
        if (mock_api) {
            out_dates = {
                "2026-08-12", "2026-08-11", "2026-08-08", "2026-08-07",
                "2026-08-06", "2026-08-05", "2026-08-04", "2026-08-01",
                "2026-07-31", "2026-07-30", "2026-07-29", "2026-07-28"
            };
            return true;
        }
        std::string cleaned_rfid_uid = clean_rfid_uid(raw_rfid_uid);

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        const std::string url = base_url + API_ATTENDANCE_PATH;

        std::string json = "{\"rfid_uid\":\"" + EscapeJSON(cleaned_rfid_uid) + "\"}";

        std::string response_data;
        struct curl_slist* headers = BuildJSONHeaders();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        ConfigureCommonOptions(curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || response_code != 200) {
            printf("- Attendance fetch failed (code=%ld, curl=%s)\n", response_code, curl_easy_strerror(res));
            if (!response_data.empty()) {
                printf("- Attendance response: %s\n", response_data.c_str());
            }
            return false;
        }

        out_dates = ParseStringArray(response_data, "dates");
        return true;
    }

    bool SendClockInWithSignature(const std::string& raw_rfid_uid, const std::string& signature_data) {
        if (mock_api) {
            const bool valid = !raw_rfid_uid.empty() && !signature_data.empty();
            printf("%c [mock API] clock-in with signature\n", valid ? '+' : '-');
            return valid;
        }

        std::string cleaned_rfid_uid = clean_rfid_uid(raw_rfid_uid);
        
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        const std::string url = base_url + "/api/clock_in_with_signature";
        
        // Build JSON with properly escaped signature data
        std::string escaped_signature = EscapeJSON(signature_data);
        std::string json = "{\"rfid_uid\":\"" + EscapeJSON(cleaned_rfid_uid) + "\",\"signature\":\"" + escaped_signature + "\"}";
        
        printf("+ Sending signed clock-in (%zu bytes)\n", json.size());
        
        // Trigger buzzer if RFID reader is available
        /*
        if (rfid_reader) {
            rfid_reader->SendBuzzCommand();

        }
            */
        

        
        std::string response_data;
        struct curl_slist* headers = BuildJSONHeaders();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        ConfigureCommonOptions(curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        
        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        printf("Response code: %ld\n", response_code);
        if (!response_data.empty()) {
            printf("Response: %s\n", response_data.c_str());
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        return (res == CURLE_OK && response_code == 200);
    }
    
    ScanResponse SendScan(const std::string& raw_rfid_uid) {
        ScanResponse response = {};
        response.success = false;
        
        std::string cleaned_rfid_uid = clean_rfid_uid(raw_rfid_uid);

        if (mock_api) {
            if (cleaned_rfid_uid == "SIM_UNKNOWN") {
                response.message = "Kaart niet geregistreerd";
                return response;
            }
            response.success = !cleaned_rfid_uid.empty();
            response.action = cleaned_rfid_uid == "SIM_CLOCK_OUT" ? "clock_out" : "clock_in";
            response.message = response.action == "clock_out" ? "Uitgeklokt" : "Klaar voor handtekening";
            response.user_name = response.action == "clock_out" ? "Simone Tester" : "Derk Simulator";
            response.user_department = "Desktop simulation";
            return response;
        }
        
        const std::string url = base_url + "/api/scan";
        const std::string json = "{\"rfid_uid\":\"" + EscapeJSON(cleaned_rfid_uid) + "\"}";
        
        // Retry up to 3 times
        for (int retry = 0; retry < 3; retry++) {
            if (retry > 0) {
                printf("- Retry %d/3...\n", retry + 1);
                usleep(1000000);
            }
            
            // Create fresh CURL handle each time
            CURL* curl = curl_easy_init();
            if (!curl) continue;
            
            std::string response_data;
            struct curl_slist* headers = BuildJSONHeaders();
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            ConfigureCommonOptions(curl);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
            
            CURLcode res = curl_easy_perform(curl);
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            
            // Check if we got ANY response from server (even error codes)
            if (res == CURLE_OK) {
                if (response_code == 200) {
                    response.success = ParseBool(response_data, "success");
                    response.action = ParseJSON(response_data, "action");
                    response.message = ParseJSON(response_data, "message");
                    
                    size_t user_pos = response_data.find("\"user\"");
                    if (user_pos != std::string::npos) {
                        std::string user_section = response_data.substr(user_pos);
                        response.user_name = ParseJSON(user_section, "name");
                        response.user_department = ParseJSON(user_section, "department");
                    }
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    return response;  // Success!
                } else if (response_code == 404) {
                    // Card not found - don't retry, return error immediately
                    response.message = "Kaart niet geregistreerd";
                    printf("- Card not found in database\n");
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    return response;
                } else if (response_code == 401 || response_code == 423) {
                    response.message = "API-autorisatie mislukt";
                    printf("- API authorization rejected (HTTP %ld)\n", response_code);
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    return response;
                } else {
                    // Other HTTP error - retry
                    printf("- HTTP error: %ld\n", response_code);
                }
            } else {
                // Connection failed - retry
                printf("- Connection error: %s\n", curl_easy_strerror(res));
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        
        // All retries failed
        response.message = "API-verbinding mislukt";
        printf("- API unavailable after retries\n");
        return response;
    }


    bool SendDirectCommand(const char* command) {
#ifdef DESKTOP_SIM
    printf("+ [simulated device] %s\n", command);
    return true;
#else
    int fd = open("/dev/ttyRPMSG0", O_WRONLY);  // Open WRITE-ONLY for commands
    if (fd < 0) {
        fprintf(stderr, "- Failed to open /dev/ttyRPMSG0: %s\n", strerror(errno));
        return false;
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s\n", command);
    
    ssize_t bytes = write(fd, cmd, strlen(cmd));
    
    close(fd);  // Close immediately after writing
    
    if (bytes < 0) {
        fprintf(stderr, "- Failed to write command: %s\n", strerror(errno));
        return false;
    }
    
    printf("+ Sent: %s", cmd);
    return true;
#endif
}


};

#endif
