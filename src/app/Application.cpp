#include "app/Application.hpp"

#include "app/ApplicationController.hpp"
#include "platform/linux/DisplayWindow.hpp"
#include "services/api/ApiWorker.hpp"
#include "services/hardware/HardwareService.hpp"
#include "services/network/NetworkService.hpp"
#include "ui/assets/UiAssets.hpp"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace clockin {
namespace {

// Hard upper bound for a clean shutdown. systemd waits TimeoutStopSec before
// SIGKILL; finishing well inside that keeps restarts fast and predictable.
constexpr double kShutdownDeadlineSeconds = 3.0;
// While the backlight is off nothing is visible, so the loop only keeps touch,
// RFID, API and network handling alive at this interval instead of rendering.
constexpr double kDarkPollSeconds = 0.04;

double SteadySeconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// Receives SIGINT/SIGTERM/SIGHUP on a dedicated thread. The signals are blocked
// in every other thread, so a blocking driver call (EGL, SPI, curl) can never
// swallow them, and a stuck cleanup is cut off after kShutdownDeadlineSeconds.
class ShutdownSignals {
public:
    ~ShutdownSignals() { Finish(); }

    bool Install() {
        sigemptyset(&signals_);
        sigaddset(&signals_, SIGINT);
        sigaddset(&signals_, SIGTERM);
        sigaddset(&signals_, SIGHUP);
        // Threads created afterwards inherit this mask, including GLFW/EGL ones.
        if (pthread_sigmask(SIG_BLOCK, &signals_, nullptr) != 0) return false;
        try {
            watcher_ = std::thread(&ShutdownSignals::Watch, this);
        } catch (...) {
            return false;
        }
        return true;
    }

    bool Requested() const { return requested_.load(); }

    void Finish() {
        finished_.store(true);
        if (watcher_.joinable()) watcher_.join();
    }

private:
    sigset_t signals_{};
    std::atomic<bool> requested_{false};
    std::atomic<bool> finished_{false};
    std::thread watcher_;

    void Watch() {
        const timespec interval{0, 100 * 1000 * 1000};
        double deadline = 0.0;
        while (!finished_.load()) {
            siginfo_t info{};
            const int signal = sigtimedwait(&signals_, &info, &interval);
            if (signal > 0) {
                if (requested_.exchange(true)) {
                    std::fprintf(stderr, "Second %s received; exiting immediately\n",
                                 strsignal(signal));
                    _exit(0);
                }
                std::printf("%s received; shutting down\n", strsignal(signal));
                deadline = SteadySeconds() + kShutdownDeadlineSeconds;
            }
            if (requested_.load() && !finished_.load() && SteadySeconds() >= deadline) {
                std::fprintf(stderr, "Shutdown exceeded %.0f s; forcing exit\n",
                             kShutdownDeadlineSeconds);
                _exit(0);
            }
        }
    }
};

}  // namespace

class Application::Impl {
public:
    explicit Impl(AppConfiguration value)
        : configuration(std::move(value)),
          api(configuration.api_base_url, configuration.api_key),
          controller(configuration, api, hardware, network) {}

    AppConfiguration configuration;
    ApiWorker api;
    HardwareService hardware;
    NetworkService network;
    ApplicationController controller;
    DisplayWindow display;
};

Application::Application(AppConfiguration configuration)
    : impl_(new Impl(std::move(configuration))) {}

Application::~Application() = default;

int Application::Run() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    ShutdownSignals shutdown;
    if (!shutdown.Install()) {
        std::fprintf(stderr, "Unable to install process signal handling\n");
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::fprintf(stderr, "Unable to initialize libcurl\n");
        return 1;
    }

    std::string display_error;
    if (!impl_->display.Open(impl_->configuration.framebuffer_format, display_error)) {
        std::fprintf(stderr, "%s\n", display_error.c_str());
        curl_global_cleanup();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.MouseDrawCursor = false;
    FontBundle fonts = AddBrandedFonts(io, 20.0f);
    if (!ImGui_ImplGlfw_InitForOpenGL(impl_->display.NativeHandle(), true) ||
        !ImGui_ImplOpenGL3_Init("#version 100")) {
        std::fprintf(stderr, "Unable to initialize the ImGui renderer\n");
        ImGui::DestroyContext();
        impl_->display.Close();
        curl_global_cleanup();
        return 1;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::EndFrame();
    InstallBrandedIcons(io, fonts, 20.0f);
    impl_->controller.SetFonts(fonts.regular, fonts.semibold, fonts.icons_installed);
    impl_->controller.SetDisplayRefresh(impl_->display.Properties().refresh_hz);

    const DisplayProperties& display = impl_->display.Properties();
    std::printf("Clock-in device ready\n");
    std::printf("Display: %s, R%d G%d B%d A%d, %d Hz\n",
                display.framebuffer_format.c_str(), display.red_bits, display.green_bits,
                display.blue_bits, display.alpha_bits, display.refresh_hz);
    std::printf("API: %s (%s)\n", impl_->configuration.api_base_url.c_str(),
                impl_->configuration.ApiReady() ? "configured" : "incomplete configuration");

    double last_frame = SteadySeconds();
    impl_->controller.Start(last_frame);
    bool dark = false;

    while (!shutdown.Requested() && !impl_->display.ShouldClose()) {
        if (dark) impl_->display.WaitEvents(kDarkPollSeconds);
        else impl_->display.PollEvents();
        const double frame_start = SteadySeconds();
        const double frame_seconds = std::max(0.0, frame_start - last_frame);
        const double delta = std::min(0.05, frame_seconds);
        last_frame = frame_start;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const double update_start = SteadySeconds();
        impl_->controller.Update(delta, frame_start);
        dark = impl_->controller.DisplayDark();
        if (dark) {
            ImGui::EndFrame();
            continue;
        }
        const double render_start = SteadySeconds();
        impl_->controller.Render(frame_start);
        ImGui::Render();

        int width = 0;
        int height = 0;
        impl_->display.FramebufferSize(width, height);
        glViewport(0, 0, width, height);
        glClearColor(247.0f / 255.0f, 251.0f / 255.0f, 1.0f, 1.0f);
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        const double swap_start = SteadySeconds();
        impl_->display.SwapBuffers();
        const double frame_end = SteadySeconds();

        ImDrawData* draw_data = ImGui::GetDrawData();
        int draw_calls = 0;
        if (draw_data != nullptr) {
            for (int index = 0; index < draw_data->CmdListsCount; ++index) {
                draw_calls += draw_data->CmdLists[index]->CmdBuffer.Size;
            }
        }
        impl_->controller.RecordFrame(
            frame_end, frame_seconds, (render_start - update_start) * 1000.0,
            (swap_start - render_start) * 1000.0, (frame_end - swap_start) * 1000.0,
            draw_data == nullptr ? 0 : draw_data->TotalVtxCount,
            draw_data == nullptr ? 0 : draw_data->TotalIdxCount, draw_calls);
    }

    const double shutdown_start = SteadySeconds();
    std::printf("Shutdown requested\n");
    impl_->controller.Stop();
    std::printf("Background services stopped\n");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    impl_->display.Close();
    curl_global_cleanup();
    std::printf("Shutdown complete in %.0f ms\n", (SteadySeconds() - shutdown_start) * 1000.0);
    shutdown.Finish();
    return 0;
}

}  // namespace clockin
