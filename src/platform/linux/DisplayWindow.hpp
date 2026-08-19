#pragma once

#include <string>

struct GLFWwindow;

namespace clockin {

struct DisplayProperties {
    std::string framebuffer_format;
    int red_bits = 0;
    int green_bits = 0;
    int blue_bits = 0;
    int alpha_bits = 0;
    int refresh_hz = 0;
};

class DisplayWindow {
public:
    DisplayWindow() = default;
    ~DisplayWindow();

    DisplayWindow(const DisplayWindow&) = delete;
    DisplayWindow& operator=(const DisplayWindow&) = delete;

    bool Open(const std::string& framebuffer_preference, std::string& error);
    void Close();
    void PollEvents() const;
    bool ShouldClose() const;
    void FramebufferSize(int& width, int& height) const;
    void SwapBuffers() const;

    GLFWwindow* NativeHandle() const { return window_; }
    const DisplayProperties& Properties() const { return properties_; }

private:
    GLFWwindow* window_ = nullptr;
    bool glfw_initialized_ = false;
    DisplayProperties properties_;
};

}  // namespace clockin
