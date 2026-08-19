#pragma once

#include <string>
#include <vector>

namespace clockin::linux_platform {

bool PathExists(const std::string& path);
std::string ResolvePath(const std::string& path);
std::vector<std::string> DirectoryEntries(const std::string& path);
std::string ErrnoMessage(const std::string& operation);
bool WriteAttribute(const std::string& path,
                    const std::string& value,
                    std::string& error,
                    int* error_number = nullptr);
std::string FindRfidSpiDevice();
std::string FindGpioDChip();
std::string FindTim16PwmChip();

}  // namespace clockin::linux_platform
