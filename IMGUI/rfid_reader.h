#ifndef RFID_READER_H
#define RFID_READER_H

#include <algorithm>
#include <deque>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#define RFID_DEV_PATH "/dev/ttyRPMSG0"

struct RFIDData {
    std::string uid;
    std::string card_type;
    std::string sak;
    bool auth_failed;
    bool valid;

    RFIDData() : auth_failed(false), valid(false) {}

    void Reset() {
        uid.clear();
        card_type.clear();
        sak.clear();
        auth_failed = false;
        valid = false;
    }
};

class RFIDReader {
public:
    RFIDReader()
        : fd_(-1),
          connected_(false),
          in_card_block_(false),
          discarding_line_(false),
          next_open_time_(0.0),
          open_backoff_(0.25),
          unavailable_reported_(false),
          ever_connected_(false),
          reconnect_count_(0),
          command_due_time_(0.0),
          command_in_flight_(false),
          command_ack_deadline_(0.0),
          last_update_time_(0.0),
          require_acknowledgements_(DefaultAcknowledgementMode()) {
        buffer_.reserve(MAX_BUFFER_BYTES);
    }

    ~RFIDReader() { Close(); }

    bool Open(double now_seconds = 0.0) {
#ifdef DESKTOP_SIM
        connected_ = true;
        (void)now_seconds;
        return true;
#else
        if (fd_ >= 0) return true;
        if (now_seconds < next_open_time_) return false;

        fd_ = open(RFID_DEV_PATH, O_RDWR | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
        if (fd_ < 0) {
            if (!unavailable_reported_) {
                fprintf(stderr, "- RFID device unavailable: %s\n", strerror(errno));
                unavailable_reported_ = true;
            }
            next_open_time_ = now_seconds + open_backoff_;
            open_backoff_ = open_backoff_ < 2.0 ? open_backoff_ * 2.0 : 2.0;
            if (open_backoff_ > 2.0) open_backoff_ = 2.0;
            return false;
        }

        struct termios tty;
        if (tcgetattr(fd_, &tty) == 0) {
            cfmakeraw(&tty);
            tty.c_cc[VMIN] = 0;
            tty.c_cc[VTIME] = 0;
            tcsetattr(fd_, TCSANOW, &tty);
        }

        if (ever_connected_) ++reconnect_count_;
        ever_connected_ = true;
        connected_ = true;
        unavailable_reported_ = false;
        open_backoff_ = 0.25;
        printf("+ RFID reader ready\n");
        return true;
#endif
    }

    void Close() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        connected_ = false;
    }

    void Update(double now_seconds) {
        last_update_time_ = now_seconds;
        if (!Open(now_seconds)) return;

#ifndef DESKTOP_SIM
        char bytes[512];
        size_t read_budget = 16 * 1024;
        while (read_budget > 0) {
            const ssize_t count = read(fd_, bytes, sizeof(bytes));
            if (count > 0) {
                FeedBytes(bytes, static_cast<size_t>(count));
                read_budget -= std::min(read_budget, static_cast<size_t>(count));
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                HandleDeviceFailure(now_seconds, "RFID read failed");
                return;
            }
            break;
        }
#endif

        ProcessCommand(now_seconds);
    }

    bool PopCard(RFIDData& card) {
        if (card_events_.empty()) return false;
        card = card_events_.front();
        card_events_.pop_front();
        return true;
    }

    bool QueueCommand(const std::string& command) {
        if (command.empty()) return false;
        const std::string channel = CommandChannel(command);
        if (!channel.empty()) {
            for (std::deque<PendingCommand>::iterator it = commands_.begin(); it != commands_.end();) {
                if (it->offset == 0 && CommandChannel(it->command) == channel) {
                    it = commands_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (!commands_.empty() && commands_.back().command == command) return true;
        if (commands_.size() >= MAX_COMMANDS) return false;
        PendingCommand pending;
        pending.command = command;
        pending.wire = command;
        if (pending.wire[pending.wire.size() - 1] != '\n') pending.wire.push_back('\n');
        commands_.push_back(pending);
        return true;
    }

    bool IsOpen() const { return connected_; }
    unsigned int ReconnectCount() const { return reconnect_count_; }
    size_t PendingCommandCount() const { return commands_.size() + (command_in_flight_ ? 1u : 0u); }

    void CancelPendingFeedback() {
        for (std::deque<PendingCommand>::iterator it = commands_.begin(); it != commands_.end();) {
            if (it->offset == 0 && (it->command == "buzz" || it->command == "beep")) {
                it = commands_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Flush() {
        buffer_.clear();
        current_data_.Reset();
        in_card_block_ = false;
        discarding_line_ = false;
        card_events_.clear();
    }

    void FeedBytesForTest(const std::string& bytes) {
        FeedBytes(bytes.data(), bytes.size());
    }

#ifdef DESKTOP_SIM
    void InjectCard(const std::string& uid) {
        if (uid.empty()) return;
        RFIDData card;
        card.uid = uid;
        card.card_type = "Simulated card";
        card.sak = "00";
        card.valid = true;
        PushCard(card);
    }

    const std::vector<std::string>& SentCommandsForTest() const { return sent_commands_; }
    void SetRequireAcknowledgementsForTest(bool enabled) { require_acknowledgements_ = enabled; }
#endif

private:
    static const size_t MAX_BUFFER_BYTES = 4096;
    static const size_t MAX_CARD_EVENTS = 8;
    static const size_t MAX_COMMANDS = 32;

    struct PendingCommand {
        std::string command;
        std::string wire;
        size_t offset;
        PendingCommand() : offset(0) {}
    };

    int fd_;
    bool connected_;
    std::string buffer_;
    RFIDData current_data_;
    bool in_card_block_;
    bool discarding_line_;
    std::deque<RFIDData> card_events_;
    double next_open_time_;
    double open_backoff_;
    bool unavailable_reported_;
    bool ever_connected_;
    unsigned int reconnect_count_;
    std::deque<PendingCommand> commands_;
    double command_due_time_;
    bool command_in_flight_;
    std::string in_flight_command_;
    double command_ack_deadline_;
    double last_update_time_;
    bool require_acknowledgements_;
#ifdef DESKTOP_SIM
    std::vector<std::string> sent_commands_;
#endif

    void HandleDeviceFailure(double now_seconds, const char* message) {
#ifndef DESKTOP_SIM
        fprintf(stderr, "- %s: %s\n", message, strerror(errno));
#else
        (void)message;
#endif
        Close();
        buffer_.clear();
        current_data_.Reset();
        in_card_block_ = false;
        discarding_line_ = false;
        command_in_flight_ = false;
        in_flight_command_.clear();
        next_open_time_ = now_seconds + open_backoff_;
        open_backoff_ = open_backoff_ < 2.0 ? open_backoff_ * 2.0 : 2.0;
        if (open_backoff_ > 2.0) open_backoff_ = 2.0;
    }

    void PushCard(const RFIDData& card) {
        if (card_events_.size() >= MAX_CARD_EVENTS) card_events_.pop_front();
        card_events_.push_back(card);
    }

    void ParseLine(const std::string& line) {
        const size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return;
        const size_t end = line.find_last_not_of(" \t\r\n");
        const std::string trimmed = line.substr(start, end - start + 1);

        if (trimmed.compare(0, 3, "RX:") == 0) {
            const size_t command_start = trimmed.find_first_not_of(" \t", 3);
            const std::string acknowledged = command_start == std::string::npos
                ? std::string()
                : trimmed.substr(command_start);
            if (command_in_flight_ && acknowledged == in_flight_command_) {
                command_in_flight_ = false;
                command_due_time_ = last_update_time_ + CommandSpacing(in_flight_command_);
                in_flight_command_.clear();
            }
            return;
        }

        if (trimmed == "=== Card Detected ===") {
            in_card_block_ = true;
            current_data_.Reset();
        } else if (trimmed == "=== End ===") {
            if (in_card_block_ && !current_data_.uid.empty()) {
                current_data_.valid = true;
                PushCard(current_data_);
            }
            current_data_.Reset();
            in_card_block_ = false;
        } else if (in_card_block_) {
            if (trimmed.compare(0, 9, "Card UID:") == 0) {
                current_data_.uid = trimmed.substr(9);
            } else if (trimmed.compare(0, 10, "Card Type:") == 0) {
                current_data_.card_type = trimmed.substr(10);
            } else if (trimmed.compare(0, 4, "SAK:") == 0) {
                current_data_.sak = trimmed.substr(4);
            } else if (trimmed.compare(0, 22, "Authentication failed!") == 0) {
                current_data_.auth_failed = true;
            }
        }
    }

    void FeedBytes(const char* bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            const char c = bytes[i];
            if (discarding_line_) {
                if (c == '\n') discarding_line_ = false;
                continue;
            }
            if (c == '\n') {
                ParseLine(buffer_);
                buffer_.clear();
                continue;
            }
            if (buffer_.size() >= MAX_BUFFER_BYTES) {
                buffer_.clear();
                current_data_.Reset();
                in_card_block_ = false;
                discarding_line_ = true;
                continue;
            }
            buffer_.push_back(c);
        }
    }

    static double CommandSpacing(const std::string& command) {
        if (command == "buzz") return 0.35;
        if (command == "beep") return 0.20;
        return 0.05;
    }

    static bool DefaultAcknowledgementMode() {
#ifdef DESKTOP_SIM
        return false;
#else
        return true;
#endif
    }

    static std::string CommandChannel(const std::string& command) {
        if (command == "red_on" || command == "red_off") return "red";
        if (command == "green_on" || command == "green_off") return "green";
        return "";
    }

    void ProcessCommand(double now_seconds) {
        if (command_in_flight_) {
            if (now_seconds < command_ack_deadline_) return;
            fprintf(stderr, "- M4 acknowledgement timed out for: %s\n", in_flight_command_.c_str());
            command_in_flight_ = false;
            in_flight_command_.clear();
            command_due_time_ = now_seconds;
        }
        if (commands_.empty() || now_seconds < command_due_time_) return;
        PendingCommand& pending = commands_.front();

#ifdef DESKTOP_SIM
        printf("+ [simulated device] %s\n", pending.command.c_str());
        sent_commands_.push_back(pending.command);
        pending.offset = pending.wire.size();
#else
        while (pending.offset < pending.wire.size()) {
            const ssize_t written = write(fd_,
                                          pending.wire.data() + pending.offset,
                                          pending.wire.size() - pending.offset);
            if (written > 0) {
                pending.offset += static_cast<size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR) continue;
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            commands_.pop_front();
            HandleDeviceFailure(now_seconds, "RFID command write failed");
            return;
        }
#endif

        if (pending.offset == pending.wire.size()) {
            if (require_acknowledgements_) {
                in_flight_command_ = pending.command;
                commands_.pop_front();
                command_in_flight_ = true;
                command_ack_deadline_ = now_seconds + 3.0;
            } else {
                const double spacing = CommandSpacing(pending.command);
                commands_.pop_front();
                command_due_time_ = now_seconds + spacing;
            }
        }
    }
};

#endif
