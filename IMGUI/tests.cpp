#include "api_worker.h"
#include "card_presence.h"
#include "performance_metrics.h"
#include "rfid_reader.h"
#include "signature_pad.h"
#include "tiny_json.h"
#include "touch_handler.h"
#include "ui_feedback.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <linux/input.h>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #condition << std::endl; \
        ++failures; \
    } \
} while (0)

input_event Event(unsigned short type, unsigned short code, int value) {
    input_event event;
    std::memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    return event;
}

float DistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length_squared = dx * dx + dy * dy;
    float t = length_squared > 0.0001f
        ? ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared
        : 0.0f;
    t = std::max(0.0f, std::min(1.0f, t));
    const float px = start.x + t * dx;
    const float py = start.y + t * dy;
    const float ex = point.x - px;
    const float ey = point.y - py;
    return std::sqrt(ex * ex + ey * ey);
}

float DistanceToPolyline(const ImVec2& point, const std::vector<ImVec2>& line) {
    float best = 100000.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        best = std::min(best, DistanceToSegment(point, line[i - 1], line[i]));
    }
    return best;
}

void TestJson() {
    const std::string source =
        "{\"success\":true,\"action\":\"clock_in\","
        "\"message\":\"Hallo \\\"Derk\\\" \\u20ac\","
        "\"user\":{\"name\":\"Jan\\nTester\",\"department\":\"ICT\"},"
        "\"dates\":[\"2026-08-12\",\"2026-08-11\"]}";
    TinyJson json(source);
    TinyJson::ObjectRange root;
    CHECK(json.RootObject(root));
    bool success = false;
    CHECK(json.GetBool(root, "success", success));
    CHECK(success);
    std::string message;
    CHECK(json.GetString(root, "message", message));
    CHECK(message == "Hallo \"Derk\" \xe2\x82\xac");
    TinyJson::ObjectRange user;
    CHECK(json.GetObject(root, "user", user));
    std::string name;
    CHECK(json.GetString(user, "name", name));
    CHECK(name == "Jan\nTester");
    std::vector<std::string> dates;
    CHECK(json.GetStringArray(root, "dates", dates));
    CHECK(dates.size() == 2);

    TinyJson malformed("{\"success\":true,,}");
    CHECK(!malformed.RootObject(root));
    TinyJson trailing("{}garbage");
    CHECK(!trailing.RootObject(root));
}

void TestSignature() {
    SignaturePad pad;
    std::vector<ImVec2> raw;
    pad.Begin(ImVec2(60.0f, 200.0f));
    raw.push_back(ImVec2(60.0f, 200.0f));
    for (int i = 1; i <= 300; ++i) {
        const ImVec2 point(60.0f + i * 1.6f, 250.0f + std::sin(i * 0.09f) * 35.0f);
        raw.push_back(point);
        pad.Add(point);
    }
    pad.End();
    CHECK(pad.IsValid());
    CHECK(!pad.TooComplex());
    CHECK(pad.TotalPointCount() < raw.size());
    CHECK(pad.Strokes().size() == 1);
    for (size_t i = 0; i < raw.size(); ++i) {
        CHECK(DistanceToPolyline(raw[i], pad.Strokes()[0]) <= 1.05f);
    }
    std::string svg;
    CHECK(pad.SerializeSVG(svg));
    CHECK(svg.size() <= SignaturePad::MAX_SVG_BYTES);
    CHECK(svg.find("<path d=\"M") != std::string::npos);

    SignaturePad tap;
    tap.Begin(ImVec2(100, 200));
    tap.End();
    CHECK(!tap.IsValid());
    CHECK(!tap.SerializeSVG(svg));

    SignaturePad tiny_marks;
    for (int i = 0; i < 3; ++i) {
        tiny_marks.Begin(ImVec2(100.0f + i * 10.0f, 200.0f));
        tiny_marks.Add(ImVec2(102.0f + i * 10.0f, 200.0f));
        tiny_marks.End();
    }
    CHECK(!tiny_marks.IsValid());
    CHECK(tiny_marks.Strokes().empty());

    SignaturePad long_line;
    long_line.Begin(ImVec2(50, 150));
    for (int i = 0; i < 2000; ++i) {
        long_line.Add(ImVec2(50.0f + (i % 550), 150.0f + ((i / 550) % 270)));
    }
    CHECK(long_line.TooComplex());
    CHECK(long_line.TotalPointCount() <= SignaturePad::MAX_POINTS);
}

void TestTouch() {
    TouchHandler touch;
    touch.SetAxisRangesForTest(0, 479, 0, 799);
    const double now = 100.0;

    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_SLOT, 0), now);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_TRACKING_ID, 4), now);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_X, 0), now);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now);
    CHECK(touch.Events().empty()); // X and Y from different reports must not be combined prematurely.
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_Y, 0), now + 0.01);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.01);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events()[0].type == TouchEventType::Down);
    CHECK(std::abs(touch.Events()[0].position.x - 0.0f) < 0.01f);
    CHECK(std::abs(touch.Events()[0].position.y - 480.0f) < 0.01f);

    touch.ClearEventsForTest();
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_X, 479), now + 0.02);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_Y, 799), now + 0.02);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.02);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events()[0].type == TouchEventType::Move);
    CHECK(std::abs(touch.Events()[0].position.x - 800.0f) < 0.01f);
    CHECK(std::abs(touch.Events()[0].position.y - 0.0f) < 0.01f);

    touch.ClearEventsForTest();
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_TRACKING_ID, -1), now + 0.025);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.025);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events()[0].type == TouchEventType::Up);
    CHECK(!touch.IsTouching());

    touch.ClearEventsForTest();
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_TRACKING_ID, 5), now + 0.026);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_X, 200), now + 0.026);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_Y, 300), now + 0.026);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.026);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events()[0].type == TouchEventType::Down);

    touch.ClearEventsForTest();
    touch.FeedEventForTest(Event(EV_SYN, SYN_DROPPED, 0), now + 0.03);
    touch.FeedEventForTest(Event(EV_ABS, ABS_MT_POSITION_X, 100), now + 0.03);
    touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.03);
    CHECK(touch.Events().size() == 1);
    CHECK(touch.Events()[0].type == TouchEventType::Cancel);
    CHECK(!touch.IsTouching());

    TouchHandler single_touch;
    single_touch.SetAxisRangesForTest(0, 479, 0, 799);
    single_touch.FeedEventForTest(Event(EV_KEY, BTN_TOUCH, 1), now + 0.04);
    single_touch.FeedEventForTest(Event(EV_ABS, ABS_X, 120), now + 0.04);
    single_touch.FeedEventForTest(Event(EV_ABS, ABS_Y, 400), now + 0.04);
    single_touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.04);
    CHECK(single_touch.Events().size() == 1);
    CHECK(single_touch.Events()[0].type == TouchEventType::Down);
    single_touch.ClearEventsForTest();
    single_touch.FeedEventForTest(Event(EV_KEY, BTN_TOUCH, 0), now + 0.05);
    single_touch.FeedEventForTest(Event(EV_SYN, SYN_REPORT, 0), now + 0.05);
    CHECK(single_touch.Events().size() == 1);
    CHECK(single_touch.Events()[0].type == TouchEventType::Up);
}

void TestRFID() {
    RFIDReader reader;
    reader.FeedBytesForTest("noise\n=== Card Det");
    reader.FeedBytesForTest("ected ===\nCard UID: 11 F3 EF 12\nCard Type: MIFARE\nSAK: 08\n=== End ===\n");
    RFIDData card;
    CHECK(reader.PopCard(card));
    CHECK(card.valid);
    CHECK(APIClient::CleanRFID(card.uid) == "11F3EF12");

    reader.FeedBytesForTest(std::string(5000, 'x'));
    reader.FeedBytesForTest("\n=== Card Detected ===\nCard UID: SECOND\n=== End ===\n");
    CHECK(reader.PopCard(card));
    CHECK(APIClient::CleanRFID(card.uid) == "SECOND");

    reader.QueueCommand("green_on");
    reader.QueueCommand("green_off"); // Coalesces the pending green state.
    reader.QueueCommand("buzz");
    reader.QueueCommand("red_on");
    reader.SetRequireAcknowledgementsForTest(true);
    reader.Update(0.0);
    CHECK(reader.SentCommandsForTest().size() == 1);
    reader.Update(0.04);
    CHECK(reader.SentCommandsForTest().size() == 1);
    reader.FeedBytesForTest("RX: wrong_command\r\n");
    reader.Update(0.10);
    CHECK(reader.SentCommandsForTest().size() == 1);
    reader.FeedBytesForTest("RX: green_off\r\n");
    reader.Update(0.149);
    CHECK(reader.SentCommandsForTest().size() == 1);
    reader.Update(0.151);
    CHECK(reader.SentCommandsForTest().size() == 2);
    reader.FeedBytesForTest("RX: buzz\r\n");
    reader.Update(0.499);
    CHECK(reader.SentCommandsForTest().size() == 2);
    reader.Update(0.502);
    CHECK(reader.SentCommandsForTest().size() == 3);
    const std::vector<std::string>& sent = reader.SentCommandsForTest();
    CHECK(sent.size() == 3);
    CHECK(sent[0] == "green_off");
    CHECK(sent[1] == "buzz");
    CHECK(sent[2] == "red_on");
}

void TestCardPresence() {
    CardPresenceTracker tracker(1.5);
    CHECK(tracker.Observe("A", 0.0));
    CHECK(!tracker.Observe("A", 0.8));
    CHECK(!tracker.Update(2.0));
    CHECK(tracker.Update(2.31));
    CHECK(!tracker.IsPresent());
    CHECK(tracker.Observe("A", 2.4));
    CHECK(tracker.Observe("B", 2.5));
}

bool WaitForResult(APIWorker& worker, ApiResult& result, int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited += 5) {
        if (worker.TryPop(result)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

void TestAPIWorker() {
    setenv("STM32_SIM_MOCK_API", "1", 1);
    setenv("STM32_SIM_API_DELAY_MS", "150", 1);
    unsetenv("STM32_SIM_API_FAILURE");
    APIWorker worker;
    CHECK(worker.Start());

    const std::chrono::steady_clock::time_point submit_start = std::chrono::steady_clock::now();
    const uint64_t scan_id = worker.SubmitScan("SIM_CLOCK_IN");
    const double submit_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - submit_start).count();
    CHECK(scan_id != 0);
    CHECK(submit_ms < 20.0); // Render-thread submission is non-blocking.
    ApiResult result;
    CHECK(WaitForResult(worker, result, 1000));
    CHECK(result.id == scan_id);
    CHECK(result.IsOk());
    CHECK(result.scan.action == "clock_in");

    setenv("STM32_SIM_API_FAILURE", "uncertain", 1);
    setenv("STM32_SIM_API_DELAY_MS", "0", 1);
    const uint64_t uncertain_id = worker.SubmitSignature("11F3EF12", "<svg/>");
    CHECK(WaitForResult(worker, result, 500));
    CHECK(result.id == uncertain_id);
    CHECK(result.outcome == ApiOutcome::Uncertain);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ApiResult unexpected_retry;
    CHECK(!worker.TryPop(unexpected_retry));

    setenv("STM32_SIM_API_FAILURE", "malformed", 1);
    const uint64_t malformed_id = worker.SubmitScan("11F3EF12");
    CHECK(WaitForResult(worker, result, 500));
    CHECK(result.id == malformed_id);
    CHECK(result.outcome == ApiOutcome::Uncertain);

    setenv("STM32_SIM_API_FAILURE", "auth", 1);
    const uint64_t auth_id = worker.SubmitScan("11F3EF12");
    CHECK(WaitForResult(worker, result, 500));
    CHECK(result.id == auth_id);
    CHECK(result.outcome == ApiOutcome::Rejected);
    CHECK(result.http_code == 401);

    setenv("STM32_SIM_API_FAILURE", "http500", 1);
    const uint64_t partial_id = worker.SubmitSignature("11F3EF12", "<svg/>");
    CHECK(WaitForResult(worker, result, 500));
    CHECK(result.id == partial_id);
    CHECK(result.outcome == ApiOutcome::Uncertain);
    CHECK(result.http_code == 500);

    setenv("STM32_SIM_API_FAILURE", "offline", 1);
    const uint64_t offline_id = worker.SubmitAttendance("11F3EF12");
    CHECK(WaitForResult(worker, result, 500));
    CHECK(result.id == offline_id);
    CHECK(result.outcome == ApiOutcome::TransportFailed);

    ApiResult stale;
    stale.id = offline_id;
    CHECK(IsCurrentApiResult(offline_id, stale));
    CHECK(!IsCurrentApiResult(offline_id + 1, stale));
    CHECK(!IsCurrentApiResult(0, stale));

    worker.Stop();

    unsetenv("STM32_SIM_API_FAILURE");
    setenv("STM32_SIM_API_DELAY_MS", "1000", 1);
    APIWorker cancellable;
    CHECK(cancellable.Start());
    cancellable.SubmitScan("SIM_CLOCK_IN");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const std::chrono::steady_clock::time_point stop_start = std::chrono::steady_clock::now();
    cancellable.Stop();
    const double stop_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - stop_start).count();
    CHECK(stop_ms < 250.0);
    unsetenv("STM32_SIM_API_DELAY_MS");
}

void TestPerformanceCadence() {
    setenv("BITS_BYTES_PERF_OVERLAY", "1", 1);
    PerformanceMetrics fifty_hz;
    double now = 0.0;
    for (int i = 0; i < 300; ++i) {
        now += 1.0 / 50.0;
        fifty_hz.Add(now, 1.0 / 50.0, 0.5, 1.0, 18.5, 4.0, 100, 150, 4);
    }
    CHECK(std::abs(fifty_hz.Snapshot().fps - 50.0) < 0.1);
    CHECK(std::abs(fifty_hz.Snapshot().frame_p95_ms - 20.0) < 0.1);

    PerformanceMetrics sixty_hz;
    now = 0.0;
    for (int i = 0; i < 300; ++i) {
        now += 1.0 / 60.0;
        sixty_hz.Add(now, 1.0 / 60.0, 0.5, 1.0, 15.1, 4.0, 100, 150, 4);
    }
    CHECK(std::abs(sixty_hz.Snapshot().fps - 60.0) < 0.1);
    CHECK(std::abs(sixty_hz.Snapshot().frame_p95_ms - 16.6667) < 0.1);
    unsetenv("BITS_BYTES_PERF_OVERLAY");
}

void TestButtonFeedback() {
    UIButtonFeedback feedback;
    CHECK(std::abs(feedback.Scale(UIControl::Admin, 10.0) - 1.0f) < 0.0001f);

    feedback.Press(UIControl::Admin);
    CHECK(feedback.IsPressed(UIControl::Admin));
    CHECK(std::abs(feedback.Scale(UIControl::Admin, 10.0) - 0.97f) < 0.0001f);
    CHECK(!feedback.IsPressed(UIControl::SignatureSubmit));

    feedback.Release(UIControl::Admin, 10.0);
    CHECK(!feedback.IsPressed(UIControl::Admin));
    CHECK(feedback.IsAnimating(UIControl::Admin, 10.05));
    CHECK(feedback.Scale(UIControl::Admin, 10.05) > 1.0f);
    CHECK(!feedback.IsAnimating(UIControl::Admin, 10.181));
    CHECK(std::abs(feedback.Scale(UIControl::Admin, 10.181) - 1.0f) < 0.0001f);

    feedback.Press(UIControl::SignatureCancel);
    feedback.CancelPress();
    CHECK(!feedback.IsPressed(UIControl::SignatureCancel));
    CHECK(std::abs(feedback.Scale(UIControl::SignatureCancel, 11.0) - 1.0f) < 0.0001f);

    feedback.Press(UIControl::AttendanceConfirm);
    feedback.Clear();
    CHECK(!feedback.IsPressed(UIControl::AttendanceConfirm));
    CHECK(!feedback.IsAnimating(UIControl::Admin, 10.1));
}

void RunWorkflowSoak(int seconds) {
    if (seconds <= 0) return;
    unsetenv("STM32_SIM_API_FAILURE");
    unsetenv("STM32_SIM_API_DELAY_MS");
    setenv("STM32_SIM_MOCK_API", "1", 1);
    APIWorker worker;
    CHECK(worker.Start());
    const std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    size_t iterations = 0;
    bool soak_ok = true;
    while (std::chrono::steady_clock::now() < end) {
        SignaturePad pad;
        for (int stroke = 0; stroke < 4; ++stroke) {
            pad.Begin(ImVec2(60.0f, 170.0f + stroke * 25.0f));
            for (int point = 1; point < 180; ++point) {
                pad.Add(ImVec2(60.0f + point * 2.8f,
                               170.0f + stroke * 25.0f + std::sin(point * 0.15f) * 12.0f));
                if (pad.TooComplex()) break;
            }
            pad.End();
        }
        std::string svg;
        if (!pad.SerializeSVG(svg)) {
            soak_ok = false;
            break;
        }

        ApiResult result;
        const std::string uid = (iterations % 2 == 0) ? "SIM_CLOCK_IN" : "SIM_CLOCK_OUT";
        const uint64_t scan_id = worker.SubmitScan(uid);
        if (scan_id == 0 || !WaitForResult(worker, result, 500) || result.id != scan_id || !result.IsOk()) {
            soak_ok = false;
            break;
        }
        if (result.scan.action == "clock_in") {
            const uint64_t attendance_id = worker.SubmitAttendance(uid);
            if (attendance_id == 0 || !WaitForResult(worker, result, 500) ||
                result.id != attendance_id || !result.IsOk()) {
                soak_ok = false;
                break;
            }
            const uint64_t signature_id = worker.SubmitSignature(uid, svg);
            if (signature_id == 0 || !WaitForResult(worker, result, 500) ||
                result.id != signature_id || !result.IsOk()) {
                soak_ok = false;
                break;
            }
        }

        // Exercise cancellation/clear without retaining the abandoned stroke.
        pad.Begin(ImVec2(100.0f, 200.0f));
        pad.Add(ImVec2(180.0f, 230.0f));
        pad.CancelStroke();
        pad.Clear();
        ++iterations;
    }
    worker.Stop();
    CHECK(soak_ok);
    std::cout << "Workflow soak iterations: " << iterations << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    int soak_seconds = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const std::string prefix = "--soak-seconds=";
        if (argument.compare(0, prefix.size(), prefix) == 0) {
            soak_seconds = std::max(0, atoi(argument.substr(prefix.size()).c_str()));
        }
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 2;
    TestJson();
    TestSignature();
    TestTouch();
    TestRFID();
    TestCardPresence();
    TestAPIWorker();
    TestPerformanceCadence();
    TestButtonFeedback();
    RunWorkflowSoak(soak_seconds);
    curl_global_cleanup();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "All tests passed" << std::endl;
    return 0;
}
