#include "services/hardware/rfid/Mfrc522.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace clockin {
namespace {

const uint8_t REG_COMMAND = 0x01;
const uint8_t REG_COMM_IEN = 0x02;
const uint8_t REG_COMM_IRQ = 0x04;
const uint8_t REG_DIV_IRQ = 0x05;
const uint8_t REG_ERROR = 0x06;
const uint8_t REG_FIFO_DATA = 0x09;
const uint8_t REG_FIFO_LEVEL = 0x0a;
const uint8_t REG_CONTROL = 0x0c;
const uint8_t REG_BIT_FRAMING = 0x0d;
const uint8_t REG_MODE = 0x11;
const uint8_t REG_TX_CONTROL = 0x14;
const uint8_t REG_TX_ASK = 0x15;
const uint8_t REG_CRC_RESULT_H = 0x21;
const uint8_t REG_CRC_RESULT_L = 0x22;
const uint8_t REG_T_MODE = 0x2a;
const uint8_t REG_T_PRESCALER = 0x2b;
const uint8_t REG_T_RELOAD_H = 0x2c;
const uint8_t REG_T_RELOAD_L = 0x2d;
const uint8_t REG_VERSION = 0x37;

const uint8_t CMD_IDLE = 0x00;
const uint8_t CMD_CALCULATE_CRC = 0x03;
const uint8_t CMD_TRANSCEIVE = 0x0c;
const uint8_t CMD_SOFT_RESET = 0x0f;

const uint8_t PICC_WUPA = 0x52;
const uint8_t PICC_CT = 0x88;
const uint8_t PICC_SELECT_CL1 = 0x93;
const uint8_t PICC_SELECT_CL2 = 0x95;
const uint8_t PICC_SELECT_CL3 = 0x97;
const uint8_t PICC_HALT = 0x50;

std::string HexUid(const std::vector<uint8_t>& bytes) {
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        stream << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return stream.str();
}

}  // namespace

Mfrc522::Mfrc522(SpiTransport& spi) : spi_(spi) {}

bool Mfrc522::WriteRegister(uint8_t reg, uint8_t value, std::string& error) {
    const uint8_t transmit[2] = {
        static_cast<uint8_t>((reg << 1) & 0x7e),
        value
    };
    uint8_t receive[2] = {0, 0};
    return spi_.Transfer(transmit, receive, sizeof(transmit), error);
}

bool Mfrc522::ReadRegister(uint8_t reg, uint8_t& value, std::string& error) {
    const uint8_t transmit[2] = {
        static_cast<uint8_t>(((reg << 1) & 0x7e) | 0x80),
        0
    };
    uint8_t receive[2] = {0, 0};
    if (!spi_.Transfer(transmit, receive, sizeof(transmit), error)) return false;
    value = receive[1];
    return true;
}

bool Mfrc522::SetBits(uint8_t reg, uint8_t mask, std::string& error) {
    uint8_t value = 0;
    return ReadRegister(reg, value, error) && WriteRegister(reg, value | mask, error);
}

bool Mfrc522::ClearBits(uint8_t reg, uint8_t mask, std::string& error) {
    uint8_t value = 0;
    return ReadRegister(reg, value, error) &&
           WriteRegister(reg, static_cast<uint8_t>(value & ~mask), error);
}

bool Mfrc522::ReadVersion(uint8_t& version, std::string& error) {
    return ReadRegister(REG_VERSION, version, error);
}

bool Mfrc522::Initialize(uint8_t& version, std::string& error) {
    if (!WriteRegister(REG_COMMAND, CMD_SOFT_RESET, error)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (!WriteRegister(REG_T_MODE, 0x8d, error) ||
        !WriteRegister(REG_T_PRESCALER, 0x3e, error) ||
        !WriteRegister(REG_T_RELOAD_L, 30, error) ||
        !WriteRegister(REG_T_RELOAD_H, 0, error) ||
        !WriteRegister(REG_TX_ASK, 0x40, error) ||
        !WriteRegister(REG_MODE, 0x3d, error)) {
        return false;
    }

    uint8_t tx_control = 0;
    if (!ReadRegister(REG_TX_CONTROL, tx_control, error)) return false;
    if ((tx_control & 0x03) == 0 && !SetBits(REG_TX_CONTROL, 0x03, error)) return false;
    return ReadVersion(version, error);
}

bool Mfrc522::CalculateCrc(const uint8_t* data,
                           size_t length,
                           uint8_t result[2],
                           std::string& error) {
    if (!ClearBits(REG_DIV_IRQ, 0x04, error) || !SetBits(REG_FIFO_LEVEL, 0x80, error)) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!WriteRegister(REG_FIFO_DATA, data[i], error)) return false;
    }
    if (!WriteRegister(REG_COMMAND, CMD_CALCULATE_CRC, error)) return false;

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(25);
    uint8_t irq = 0;
    do {
        if (!ReadRegister(REG_DIV_IRQ, irq, error)) return false;
        if (irq & 0x04) break;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    } while (std::chrono::steady_clock::now() < deadline);

    if (!(irq & 0x04)) {
        error = "MFRC522 CRC calculation timed out";
        return false;
    }
    return ReadRegister(REG_CRC_RESULT_L, result[0], error) &&
           ReadRegister(REG_CRC_RESULT_H, result[1], error);
}

Mfrc522::TransferResult Mfrc522::Transceive(const uint8_t* transmit,
                                             size_t transmit_length,
                                             std::vector<uint8_t>& receive,
                                             size_t& receive_bits,
                                             std::string& error) {
    receive.clear();
    receive_bits = 0;
    if (!WriteRegister(REG_COMM_IEN, 0xf7, error) ||
        !ClearBits(REG_COMM_IRQ, 0x80, error) ||
        !SetBits(REG_FIFO_LEVEL, 0x80, error) ||
        !WriteRegister(REG_COMMAND, CMD_IDLE, error)) {
        return TransferFailed;
    }
    for (size_t i = 0; i < transmit_length; ++i) {
        if (!WriteRegister(REG_FIFO_DATA, transmit[i], error)) return TransferFailed;
    }
    if (!WriteRegister(REG_COMMAND, CMD_TRANSCEIVE, error) ||
        !SetBits(REG_BIT_FRAMING, 0x80, error)) {
        return TransferFailed;
    }

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
    uint8_t irq = 0;
    do {
        if (!ReadRegister(REG_COMM_IRQ, irq, error)) return TransferFailed;
        if ((irq & 0x30) || (irq & 0x01)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    } while (std::chrono::steady_clock::now() < deadline);

    if (!ClearBits(REG_BIT_FRAMING, 0x80, error)) return TransferFailed;
    if (irq & 0x01) return TransferNoCard;
    if (!(irq & 0x30)) return TransferNoCard;

    uint8_t register_error = 0;
    if (!ReadRegister(REG_ERROR, register_error, error)) return TransferFailed;
    if (register_error & 0x1b) {
        error = "MFRC522 reported a framing, parity, protocol, or collision error";
        return TransferFailed;
    }

    uint8_t fifo_length = 0;
    uint8_t control = 0;
    if (!ReadRegister(REG_FIFO_LEVEL, fifo_length, error) ||
        !ReadRegister(REG_CONTROL, control, error)) {
        return TransferFailed;
    }
    if (fifo_length > 64) {
        error = "MFRC522 returned an oversized FIFO response";
        return TransferFailed;
    }
    const uint8_t final_bits = control & 0x07;
    receive_bits = final_bits == 0
        ? static_cast<size_t>(fifo_length) * 8
        : static_cast<size_t>(fifo_length == 0 ? 0 : fifo_length - 1) * 8 + final_bits;
    receive.reserve(fifo_length);
    for (uint8_t i = 0; i < fifo_length; ++i) {
        uint8_t value = 0;
        if (!ReadRegister(REG_FIFO_DATA, value, error)) return TransferFailed;
        receive.push_back(value);
    }
    return TransferOk;
}

Mfrc522::TransferResult Mfrc522::Request(std::string& error) {
    if (!WriteRegister(REG_BIT_FRAMING, 0x07, error)) return TransferFailed;
    const uint8_t request = PICC_WUPA;
    std::vector<uint8_t> response;
    size_t response_bits = 0;
    const TransferResult result = Transceive(&request, 1, response, response_bits, error);
    if (result != TransferOk) return result;
    return response_bits == 16 ? TransferOk : TransferNoCard;
}

Mfrc522::TransferResult Mfrc522::ReadCascadeLevel(uint8_t select_command,
                                                   std::vector<uint8_t>& cascade_uid,
                                                   uint8_t& sak,
                                                   std::string& error) {
    if (!WriteRegister(REG_BIT_FRAMING, 0x00, error)) return TransferFailed;
    const uint8_t anticollision[2] = {select_command, 0x20};
    size_t response_bits = 0;
    TransferResult result = Transceive(anticollision, sizeof(anticollision),
                                       cascade_uid, response_bits, error);
    if (result != TransferOk) return result;
    if (response_bits != 40 || cascade_uid.size() != 5) {
        error = "MFRC522 anticollision response had an invalid length";
        return TransferFailed;
    }
    const uint8_t checksum = static_cast<uint8_t>(
        cascade_uid[0] ^ cascade_uid[1] ^ cascade_uid[2] ^ cascade_uid[3]);
    if (checksum != cascade_uid[4]) {
        error = "MFRC522 UID checksum did not match";
        return TransferFailed;
    }

    uint8_t select[9] = {
        select_command, 0x70,
        cascade_uid[0], cascade_uid[1], cascade_uid[2], cascade_uid[3], cascade_uid[4],
        0, 0
    };
    if (!CalculateCrc(select, 7, &select[7], error)) return TransferFailed;
    std::vector<uint8_t> select_response;
    response_bits = 0;
    result = Transceive(select, sizeof(select), select_response, response_bits, error);
    if (result != TransferOk) return result;
    if (response_bits != 24 || select_response.size() != 3) {
        error = "MFRC522 select response had an invalid length";
        return TransferFailed;
    }
    uint8_t crc[2] = {0, 0};
    if (!CalculateCrc(&select_response[0], 1, crc, error)) return TransferFailed;
    if (crc[0] != select_response[1] || crc[1] != select_response[2]) {
        error = "MFRC522 select response CRC did not match";
        return TransferFailed;
    }
    sak = select_response[0];
    return TransferOk;
}

Mfrc522::PollResult Mfrc522::PollUid(std::string& uid, std::string& error) {
    uid.clear();
    error.clear();
    const TransferResult request = Request(error);
    if (request == TransferNoCard) return NoCard;
    if (request == TransferFailed) return CommunicationError;

    const uint8_t cascade_commands[3] = {
        PICC_SELECT_CL1, PICC_SELECT_CL2, PICC_SELECT_CL3
    };
    std::vector<uint8_t> complete_uid;
    for (size_t level = 0; level < 3; ++level) {
        std::vector<uint8_t> cascade_uid;
        uint8_t sak = 0;
        const TransferResult result = ReadCascadeLevel(cascade_commands[level],
                                                       cascade_uid, sak, error);
        if (result == TransferNoCard) return NoCard;
        if (result == TransferFailed) return CommunicationError;

        const bool has_cascade_tag = cascade_uid[0] == PICC_CT;
        const size_t start = has_cascade_tag ? 1 : 0;
        for (size_t i = start; i < 4; ++i) complete_uid.push_back(cascade_uid[i]);

        if ((sak & 0x04) == 0) {
            uid = HexUid(complete_uid);
            Halt();
            return uid.empty() ? CommunicationError : CardFound;
        }
        if (!has_cascade_tag) {
            error = "MFRC522 requested another cascade level without a cascade tag";
            return CommunicationError;
        }
    }

    error = "MFRC522 UID exceeded three cascade levels";
    return CommunicationError;
}

void Mfrc522::Halt() {
    uint8_t halt[4] = {PICC_HALT, 0, 0, 0};
    std::string ignored_error;
    if (!CalculateCrc(halt, 2, &halt[2], ignored_error)) return;
    std::vector<uint8_t> ignored_response;
    size_t ignored_bits = 0;
    Transceive(halt, sizeof(halt), ignored_response, ignored_bits, ignored_error);
}

}  // namespace clockin
