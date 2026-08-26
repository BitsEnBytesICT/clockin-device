#include "platform/linux/LinuxGpioOutput.hpp"
#include "platform/linux/LinuxPwm.hpp"
#include "platform/linux/LinuxPwmBuzzerOutput.hpp"
#include "platform/linux/LinuxSpiDevice.hpp"
#include "services/hardware/buzzer/BuzzerSequencer.hpp"
#include "services/hardware/rfid/Mfrc522.hpp"

#include <atomic>
#include <chrono>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

namespace {

using clockin::LinuxGpioOutput;
using clockin::LinuxPwm;
using clockin::LinuxPwmBuzzerOutput;
using clockin::LinuxSpiDevice;
using clockin::Mfrc522;
using clockin::PulseMfrc522Reset;

std::atomic<bool> keep_running(true);

void HandleSignal(int) {
    keep_running.store(false);
}

double Seconds(const std::chrono::steady_clock::time_point& value) {
    return std::chrono::duration<double>(value.time_since_epoch()).count();
}

bool PlayCue(clockin::BuzzerSequencer& buzzer,
             clockin::BuzzerPattern pattern,
             const char* label,
             std::string& error) {
    printf("BUZZER: %s\n", label);
    if (!buzzer.Start(pattern, Seconds(std::chrono::steady_clock::now()), error)) {
        return false;
    }
    while (buzzer.Active() && keep_running.load()) {
        if (!buzzer.Update(Seconds(std::chrono::steady_clock::now()), error)) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    return true;
}

bool TestBuzzer() {
    LinuxPwm pwm;
    LinuxPwmBuzzerOutput output(pwm);
    clockin::BuzzerSequencer buzzer(output);
    std::string error;
    if (!PlayCue(buzzer, clockin::BuzzerPattern::CardAccepted,
                 "subtle card acknowledgement", error) ||
        !PlayCue(buzzer, clockin::BuzzerPattern::Success,
                 "rising success cue", error) ||
        !PlayCue(buzzer, clockin::BuzzerPattern::Warning,
                 "restrained warning cue", error) ||
        !PlayCue(buzzer, clockin::BuzzerPattern::Error,
                 "descending error cue", error)) {
        fprintf(stderr, "BUZZER FAIL: %s\n", error.c_str());
        return false;
    }
    printf("BUZZER PASS: PWM is disabled\n");
    return true;
}

bool TestRfid(int seconds) {
    LinuxSpiDevice spi;
    LinuxGpioOutput reset;
    std::string error;
    if (!spi.Open(error) || !PulseMfrc522Reset(reset, error)) {
        fprintf(stderr, "RFID FAIL: %s\n", error.c_str());
        return false;
    }

    Mfrc522 reader(spi);
    uint8_t version = 0;
    if (!reader.Initialize(version, error)) {
        fprintf(stderr, "RFID FAIL: %s\n", error.c_str());
        return false;
    }
    printf("RFID: %s, MFRC522 version register 0x%02X\n",
           spi.DevicePath().c_str(), version);
    if (version == 0x00 || version == 0xff) {
        fprintf(stderr, "RFID FAIL: version indicates a floating or unavailable SPI bus\n");
        return false;
    }
    if (version != 0x91 && version != 0x92) {
        printf("RFID WARNING: nonstandard clone version; UID scanning will still be tested\n");
    }

    printf("RFID: present and remove cards for %d seconds (Ctrl+C to stop)\n", seconds);
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    std::string present_uid;
    std::chrono::steady_clock::time_point last_seen = std::chrono::steady_clock::now();
    unsigned int presentations = 0;
    int consecutive_errors = 0;
    while (keep_running.load() && std::chrono::steady_clock::now() < deadline) {
        std::string uid;
        error.clear();
        const Mfrc522::PollResult result = reader.PollUid(uid, error);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (result == Mfrc522::CardFound) {
            consecutive_errors = 0;
            if (present_uid.empty() || present_uid != uid) {
                present_uid = uid;
                ++presentations;
                printf("RFID UID %u: %s\n", presentations, uid.c_str());
                fflush(stdout);
            }
            last_seen = now;
        } else if (result == Mfrc522::NoCard) {
            consecutive_errors = 0;
            if (!present_uid.empty() &&
                now - last_seen >= std::chrono::milliseconds(500)) {
                printf("RFID: card removed\n");
                present_uid.clear();
            }
        } else {
            ++consecutive_errors;
            if (consecutive_errors >= 5) {
                fprintf(stderr, "RFID FAIL: %s\n", error.c_str());
                return false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    printf("RFID SUMMARY: %u card presentation(s)\n", presentations);
    return presentations > 0;
}

} // namespace

int main(int argc, char** argv) {
    bool test_buzzer = true;
    bool test_rfid = true;
    int rfid_seconds = 60;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--buzzer-only") test_rfid = false;
        else if (argument == "--rfid-only") test_buzzer = false;
        else if (argument.compare(0, 15, "--rfid-seconds=") == 0) {
            rfid_seconds = atoi(argument.substr(15).c_str());
            if (rfid_seconds < 1) rfid_seconds = 1;
        } else {
            fprintf(stderr,
                    "Usage: %s [--buzzer-only|--rfid-only] [--rfid-seconds=N]\n",
                    argv[0]);
            return 2;
        }
    }

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);
    bool success = true;
    if (test_buzzer) success = TestBuzzer() && success;
    if (test_rfid && keep_running.load()) success = TestRfid(rfid_seconds) && success;
    return success ? 0 : 1;
}
