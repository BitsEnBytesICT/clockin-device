#pragma once

#include <memory>
#include <string>

namespace clockin {

class LinuxGpioOutput {
public:
    LinuxGpioOutput();
    ~LinuxGpioOutput();
    bool OpenGpioD(unsigned int offset, bool initial_high, std::string& error);
    bool Set(bool high, std::string& error);
    void Close();
    bool IsOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool PulseMfrc522Reset(LinuxGpioOutput& reset, std::string& error);

}  // namespace clockin
