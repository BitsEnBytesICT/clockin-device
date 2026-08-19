#include "services/hardware/HardwareService.hpp"

#include "platform/linux/LinuxGpioOutput.hpp"
#include "platform/linux/LinuxPwm.hpp"
#include "platform/linux/LinuxSpiDevice.hpp"
#include "services/hardware/rfid/Mfrc522.hpp"
#include "services/hardware/buzzer/BuzzerSequencer.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace clockin {
namespace {

constexpr std::size_t kMaxCardEvents = 16;
constexpr std::size_t kMaxFeedbackEvents = 16;
enum class FeedbackEvent { Beep, Buzz };

double Seconds(const std::chrono::steady_clock::time_point& value) {
    return std::chrono::duration<double>(value.time_since_epoch()).count();
}

class PwmBuzzerOutput final : public IBuzzerOutput {
public:
    explicit PwmBuzzerOutput(LinuxPwm& pwm) : pwm_(pwm) {}

    bool SetEnabled(bool enabled, std::string& error) override {
        if (enabled && ((!pwm_.IsOpen() && !pwm_.OpenTim16Channel1(error)) ||
                        !pwm_.Configure(500000, 100000, error))) {
            return false;
        }
        return pwm_.SetEnabled(enabled, error);
    }

private:
    LinuxPwm& pwm_;
};

}  // namespace

class HardwareService::Impl {
public:
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    std::atomic<bool> rfid_open{false};
    std::atomic<unsigned int> reconnect_count{0};
    std::atomic<unsigned int> cancel_generation{0};
    mutable std::mutex mutex;
    std::deque<RfidCard> cards;
    std::deque<FeedbackEvent> feedback;
    std::string last_error;
    std::thread worker;

    void SetError(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        last_error = error;
    }

    void PushCard(const std::string& uid) {
        std::lock_guard<std::mutex> lock(mutex);
        if (cards.size() >= kMaxCardEvents) cards.pop_front();
        cards.push_back({uid, !uid.empty()});
    }

    bool PopFeedback(FeedbackEvent& event) {
        std::lock_guard<std::mutex> lock(mutex);
        if (feedback.empty()) return false;
        event = feedback.front();
        feedback.pop_front();
        return true;
    }

    void Run();
};

void HardwareService::Impl::Run() {
    LinuxSpiDevice spi;
    LinuxGpioOutput reset;
    LinuxPwm pwm;
    PwmBuzzerOutput buzzer_output(pwm);
    BuzzerSequencer buzzer(buzzer_output);
    Mfrc522 reader(spi);
    bool ever_connected = false;
    bool reader_ready = false;
    int consecutive_reader_errors = 0;
    auto next_rfid_open = std::chrono::steady_clock::now();
    auto next_scan = std::chrono::steady_clock::now();
    unsigned int observed_cancel_generation = cancel_generation.load();

    while (running.load()) {
        const auto now = std::chrono::steady_clock::now();
        const unsigned int current_cancel_generation = cancel_generation.load();
        if (current_cancel_generation != observed_cancel_generation) {
            observed_cancel_generation = current_cancel_generation;
            buzzer.Cancel();
        }
        std::string buzzer_error;
        if (!buzzer.Update(Seconds(now), buzzer_error)) {
            SetError(buzzer_error);
            std::fprintf(stderr, "Buzzer unavailable: %s\n", buzzer_error.c_str());
        }
        if (!buzzer.Active()) {
            FeedbackEvent event;
            if (PopFeedback(event)) {
                std::string error;
                const BuzzerPattern pattern = event == FeedbackEvent::Buzz
                                                  ? BuzzerPattern::Triple
                                                  : BuzzerPattern::Single;
                if (!buzzer.Start(pattern, Seconds(now), error)) {
                    SetError(error);
                    std::fprintf(stderr, "Buzzer unavailable: %s\n", error.c_str());
                }
            }
        }
        if (!reader_ready && now >= next_rfid_open) {
            std::string error;
            std::uint8_t version = 0;
            if (spi.Open(error) && PulseMfrc522Reset(reset, error) &&
                reader.Initialize(version, error) && version != 0x00 && version != 0xff) {
                reader_ready = true;
                rfid_open.store(true);
                consecutive_reader_errors = 0;
                if (ever_connected) ++reconnect_count;
                ever_connected = true;
                std::printf("MFRC522 ready on %s (version 0x%02X)\n",
                            spi.DevicePath().c_str(), version);
            } else {
                if (error.empty()) {
                    char message[80];
                    std::snprintf(message, sizeof(message),
                                  "MFRC522 returned invalid version 0x%02X", version);
                    error = message;
                }
                SetError(error);
                std::fprintf(stderr, "RFID unavailable: %s\n", error.c_str());
                rfid_open.store(false);
                reset.Close();
                spi.Close();
                next_rfid_open = now + std::chrono::seconds(2);
            }
        }
        if (reader_ready && now >= next_scan) {
            std::string uid;
            std::string error;
            const Mfrc522::PollResult result = reader.PollUid(uid, error);
            next_scan = now + std::chrono::milliseconds(100);
            if (result == Mfrc522::CardFound) {
                consecutive_reader_errors = 0;
                PushCard(uid);
            } else if (result == Mfrc522::CommunicationError) {
                if (++consecutive_reader_errors >= 5) {
                    SetError(error);
                    std::fprintf(stderr, "RFID communication lost: %s\n", error.c_str());
                    reader_ready = false;
                    rfid_open.store(false);
                    reset.Close();
                    spi.Close();
                    next_rfid_open = now + std::chrono::seconds(2);
                }
            } else {
                consecutive_reader_errors = 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    buzzer.Cancel();
    rfid_open.store(false);
    reset.Close();
    spi.Close();
}

HardwareService::HardwareService() : impl_(new Impl) {}
HardwareService::~HardwareService() { Stop(); }

void HardwareService::Update(double) {
    bool expected = false;
    if (!impl_->started.compare_exchange_strong(expected, true)) return;
    impl_->running.store(true);
    impl_->worker = std::thread(&Impl::Run, impl_.get());
}

void HardwareService::Stop() {
    if (!impl_->started.load()) return;
    impl_->running.store(false);
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_->started.store(false);
    impl_->rfid_open.store(false);
}

bool HardwareService::PopCard(RfidCard& card) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->cards.empty()) return false;
    card = std::move(impl_->cards.front());
    impl_->cards.pop_front();
    return true;
}

void HardwareService::Beep() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->feedback.size() < kMaxFeedbackEvents) {
        impl_->feedback.push_back(FeedbackEvent::Beep);
    }
}

void HardwareService::Buzz() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->feedback.size() < kMaxFeedbackEvents) {
        impl_->feedback.push_back(FeedbackEvent::Buzz);
    }
}

void HardwareService::CancelFeedback() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->feedback.clear();
    ++impl_->cancel_generation;
}

bool HardwareService::RfidOpen() const { return impl_->rfid_open.load(); }
unsigned int HardwareService::ReconnectCount() const { return impl_->reconnect_count.load(); }

std::size_t HardwareService::PendingFeedbackCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->feedback.size();
}

std::string HardwareService::LastError() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->last_error;
}

}  // namespace clockin
