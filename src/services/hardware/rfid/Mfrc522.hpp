#pragma once

#include "services/hardware/rfid/SpiTransport.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace clockin {

class Mfrc522 {
public:
    enum PollResult {
        CardFound,
        NoCard,
        CommunicationError
    };

    explicit Mfrc522(SpiTransport& spi);

    bool Initialize(uint8_t& version, std::string& error);
    bool ReadVersion(uint8_t& version, std::string& error);
    PollResult PollUid(std::string& uid, std::string& error);

private:
    enum TransferResult {
        TransferOk,
        TransferNoCard,
        TransferFailed
    };

    SpiTransport& spi_;

    bool WriteRegister(uint8_t reg, uint8_t value, std::string& error);
    bool ReadRegister(uint8_t reg, uint8_t& value, std::string& error);
    bool SetBits(uint8_t reg, uint8_t mask, std::string& error);
    bool ClearBits(uint8_t reg, uint8_t mask, std::string& error);
    bool CalculateCrc(const uint8_t* data, size_t length, uint8_t result[2], std::string& error);
    TransferResult Transceive(const uint8_t* transmit,
                              size_t transmit_length,
                              std::vector<uint8_t>& receive,
                              size_t& receive_bits,
                              std::string& error);
    TransferResult Request(std::string& error);
    TransferResult ReadCascadeLevel(uint8_t select_command,
                                    std::vector<uint8_t>& cascade_uid,
                                    uint8_t& sak,
                                    std::string& error);
    void Halt();
};

}  // namespace clockin
