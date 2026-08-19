#include "platform/linux/DisplayWindow.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>

namespace clockin {
namespace {

void ReportGlfwError(int, const char* description) {
    std::fprintf(stderr, "GLFW: %s\n", description == nullptr ? "unknown error" : description);
}

void SetWindowHints(int red, int green, int blue, int alpha) {
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, red);
    glfwWindowHint(GLFW_GREEN_BITS, green);
    glfwWindowHint(GLFW_BLUE_BITS, blue);
    glfwWindowHint(GLFW_ALPHA_BITS, alpha);
}

}  // namespace

DisplayWindow::~DisplayWindow() { Close(); }

bool DisplayWindow::Open(const std::string& preference, std::string& error) {
    if (window_ != nullptr) return true;
    glfwSetErrorCallback(ReportGlfwError);
    if (glfwInit() != GLFW_TRUE) {
        error = "Unable to initialize GLFW/Wayland";
        return false;
    }
    glfw_initialized_ = true;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor == nullptr) {
        error = "No active display was found";
        Close();
        return false;
    }

    if (preference == "auto" || preference == "rgb565") {
        SetWindowHints(5, 6, 5, 0);
        window_ = glfwCreateWindow(800, 480, "RFID Attendance", monitor, nullptr);
        if (window_ != nullptr) properties_.framebuffer_format = "rgb565";
        else std::fprintf(stderr, "RGB565 unavailable; falling back to RGBA8888\n");
    }
    if (window_ == nullptr) {
        SetWindowHints(8, 8, 8, 8);
        window_ = glfwCreateWindow(800, 480, "RFID Attendance", monitor, nullptr);
        if (window_ != nullptr) properties_.framebuffer_format = "rgba8888";
    }
    if (window_ == nullptr) {
        error = "Unable to create the 800x480 EGL fullscreen window";
        Close();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glGetIntegerv(GL_RED_BITS, &properties_.red_bits);
    glGetIntegerv(GL_GREEN_BITS, &properties_.green_bits);
    glGetIntegerv(GL_BLUE_BITS, &properties_.blue_bits);
    glGetIntegerv(GL_ALPHA_BITS, &properties_.alpha_bits);
    if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
        properties_.refresh_hz = mode->refreshRate;
    }
    return true;
}

void DisplayWindow::Close() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }
}

void DisplayWindow::PollEvents() const { glfwPollEvents(); }

bool DisplayWindow::ShouldClose() const {
    return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void DisplayWindow::FramebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

void DisplayWindow::SwapBuffers() const { glfwSwapBuffers(window_); }

}  // namespace clockin
