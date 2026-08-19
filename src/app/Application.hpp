#pragma once

#include "app/AppConfiguration.hpp"

#include <memory>

namespace clockin {

class Application {
public:
    explicit Application(AppConfiguration configuration);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace clockin
