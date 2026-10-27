#include "services/hardware/HardwareService.hpp"

#include "platform/linux/LinuxGpioOutput.hpp"
#include "platform/linux/LinuxPwm.hpp"
#include "platform/linux/LinuxPwmBuzzerOutput.hpp"
#include "platform/linux/LinuxSpiDevice.hpp"
#include "services/hardware/rfid/Mfrc522.hpp"
#include "services/hardware/buzzer/BuzzerSequencer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace clockin {
namespace {

using SteadyClock = std::chrono::steady_clock;

constexpr std::size_t kMaxCardEvents = 16;
constexpr std::size_t kMaxFeedbackEvents = 6;
constexpr auto kMaximumIdleWait = std::chrono::milliseconds(100);

double Seconds(const SteadyClock::time_point& value) {
    return std::chrono::duration<double>(value.time_since_epoch()).count();
}

SteadyClock::time_point TimePoint(double seconds) {
    return SteadyClock::time_point(std::chrono::duration_cast<SteadyClock::duration>(
        std::chrono::duration<double>(seconds)));
}

int FeedbackPriority(FeedbackCue cue) {
    switch (cue) {
        case FeedbackCue::Tap: return 0;
        case FeedbackCue::CardAccepted: return 1;
        case FeedbackCue::Warning: return 2;
        case FeedbackCue::Success: return 3;
        case FeedbackCue::Error: return 4;
    }
    return 0;
}

BuzzerPattern PatternFor(FeedbackCue cue) {
    switch (cue) {
        case FeedbackCue::Tap: return BuzzerPattern::Tap;
        case FeedbackCue::CardAccepted: return BuzzerPattern::CardAccepted;
        case FeedbackCue::Success: return BuzzerPattern::Success;
        case FeedbackCue::Warning: return BuzzerPattern::Warning;
        case FeedbackCue::Error: return BuzzerPattern::Error;
    }
    return BuzzerPattern::Tap;
}

}  // namespace

class HardwareService::Impl {
public:
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    std::atomic<bool> rfid_open{false};
    std::atomic<unsigned int> reconnect_count{0};
    std::atomic<unsigned int> cancel_generation{0};
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<RfidCard> cards;
    std::deque<FeedbackCue> feedback;
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

    bool PopFeedback(FeedbackCue& event) {
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
    LinuxPwmBuzzerOutput buzzer_output(pwm);
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
            FeedbackCue event;
            if (PopFeedback(event)) {
                std::string error;
                if (!buzzer.Start(PatternFor(event), Seconds(now), error)) {
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
        // Sleep until the next RFID poll, reader retry or buzzer phase change
        // instead of spinning; queued feedback and Stop() wake the thread early.
        auto wake_at = std::min(reader_ready ? next_scan : next_rfid_open,
                                SteadyClock::now() + kMaximumIdleWait);
        if (buzzer.Active()) wake_at = std::min(wake_at, TimePoint(buzzer.Deadline()));
        const bool buzzer_busy = buzzer.Active();
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait_until(lock, wake_at, [&] {
            return !running.load() ||
                   cancel_generation.load() != observed_cancel_generation ||
                   (!buzzer_busy && !feedback.empty());
        });
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
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->running.store(false);
    }
    impl_->wake.notify_all();
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

void HardwareService::PlayFeedback(FeedbackCue cue) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (cue == FeedbackCue::Tap && !impl_->feedback.empty()) return;
    if (!impl_->feedback.empty() && impl_->feedback.back() == cue) return;

    const int priority = FeedbackPriority(cue);
    if (priority >= FeedbackPriority(FeedbackCue::Warning)) {
        for (auto item = impl_->feedback.begin(); item != impl_->feedback.end();) {
            if (FeedbackPriority(*item) < priority) item = impl_->feedback.erase(item);
            else ++item;
        }
    }
    if (impl_->feedback.size() >= kMaxFeedbackEvents) {
        if (priority < FeedbackPriority(impl_->feedback.front())) return;
        impl_->feedback.pop_front();
    }
    impl_->feedback.push_back(cue);
    impl_->wake.notify_one();
}

void HardwareService::CancelFeedback() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->feedback.clear();
    ++impl_->cancel_generation;
    impl_->wake.notify_one();
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
