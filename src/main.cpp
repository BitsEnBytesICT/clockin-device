#include "app/AppConfiguration.hpp"
#include "app/Application.hpp"

#include <cstdio>
#include <utility>
#include <vector>

int main() {
    std::vector<std::string> warnings;
    clockin::AppConfiguration configuration =
        clockin::AppConfiguration::Load(&warnings);
    for (const std::string& warning : warnings) {
        std::fprintf(stderr, "Configuration: %s\n", warning.c_str());
    }
    clockin::Application application(std::move(configuration));
    return application.Run();
}
