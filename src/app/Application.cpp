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
#include <chrono>
#include <csignal>
#include <cstdio>
#include <utility>

namespace clockin {
namespace {

volatile std::sig_atomic_t g_exit_requested = 0;

void HandleSignal(int) { g_exit_requested = 1; }

bool InstallSignalHandlers() {
    struct sigaction action {};
    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(SIGINT, &action, nullptr) == 0 &&
           sigaction(SIGTERM, &action, nullptr) == 0;
}

double SteadySeconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

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

    g_exit_requested = 0;
    if (!InstallSignalHandlers()) {
        std::fprintf(stderr, "Unable to install process signal handlers\n");
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        impl_->display.Close();
        curl_global_cleanup();
        return 1;
    }
    double last_frame = SteadySeconds();
    impl_->controller.Start(last_frame);

    while (g_exit_requested == 0 && !impl_->display.ShouldClose()) {
        const double frame_start = SteadySeconds();
        const double frame_seconds = std::max(0.0, frame_start - last_frame);
        const double delta = std::min(0.05, frame_seconds);
        last_frame = frame_start;

        impl_->display.PollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const double update_start = SteadySeconds();
        impl_->controller.Update(delta, frame_start);
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

    std::printf("Shutdown requested\n");
    impl_->controller.Stop();
    std::printf("Background services stopped\n");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    impl_->display.Close();
    curl_global_cleanup();
    return 0;
}

}  // namespace clockin
