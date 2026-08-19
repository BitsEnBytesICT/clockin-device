#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace clockin {

class SpiTransport {
public:
    virtual ~SpiTransport() = default;

    virtual bool Transfer(const std::uint8_t* transmit,
                          std::uint8_t* receive,
                          std::size_t length,
                          std::string& error) = 0;
};

}  // namespace clockin
