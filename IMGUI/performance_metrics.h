#ifndef PERFORMANCE_METRICS_H
#define PERFORMANCE_METRICS_H

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

struct PerformanceSnapshot {
    double fps;
    double frame_p50_ms;
    double frame_p95_ms;
    double frame_p99_ms;
    double update_p95_ms;
    double render_p95_ms;
    double swap_p95_ms;
    double touch_latency_ms;
    int vertices;
    int indices;
    int draw_calls;

    PerformanceSnapshot()
        : fps(0.0), frame_p50_ms(0.0), frame_p95_ms(0.0), frame_p99_ms(0.0),
          update_p95_ms(0.0), render_p95_ms(0.0), swap_p95_ms(0.0),
          touch_latency_ms(-1.0), vertices(0), indices(0), draw_calls(0) {}
};

class PerformanceMetrics {
public:
    PerformanceMetrics()
        : enabled_(ReadEnabled()), cursor_(0), full_(false), last_refresh_(0.0) {
        samples_.resize(300);
        frame_scratch_.reserve(samples_.size());
        update_scratch_.reserve(samples_.size());
        render_scratch_.reserve(samples_.size());
        swap_scratch_.reserve(samples_.size());
    }

    bool Enabled() const { return enabled_; }

    void Add(double now_seconds,
             double frame_seconds,
             double update_ms,
             double render_ms,
             double swap_ms,
             double touch_latency_ms,
             int vertices,
             int indices,
             int draw_calls) {
        Sample& sample = samples_[cursor_];
        sample.frame_ms = frame_seconds * 1000.0;
        sample.update_ms = update_ms;
        sample.render_ms = render_ms;
        sample.swap_ms = swap_ms;
        cursor_ = (cursor_ + 1) % samples_.size();
        if (cursor_ == 0) full_ = true;

        if (touch_latency_ms >= 0.0) snapshot_.touch_latency_ms = touch_latency_ms;
        snapshot_.vertices = vertices;
        snapshot_.indices = indices;
        snapshot_.draw_calls = draw_calls;
        if (enabled_ && now_seconds - last_refresh_ >= 0.5) {
            RefreshSnapshot();
            last_refresh_ = now_seconds;
        }
    }

    const PerformanceSnapshot& Snapshot() const { return snapshot_; }

private:
    struct Sample {
        double frame_ms;
        double update_ms;
        double render_ms;
        double swap_ms;
        Sample() : frame_ms(0.0), update_ms(0.0), render_ms(0.0), swap_ms(0.0) {}
    };

    bool enabled_;
    std::vector<Sample> samples_;
    size_t cursor_;
    bool full_;
    double last_refresh_;
    PerformanceSnapshot snapshot_;
    std::vector<double> frame_scratch_;
    std::vector<double> update_scratch_;
    std::vector<double> render_scratch_;
    std::vector<double> swap_scratch_;

    static bool ReadEnabled() {
        const char* value = std::getenv("BITS_BYTES_PERF_OVERLAY");
        return value != NULL && std::string(value) == "1";
    }

    static double Percentile(std::vector<double>& values, double percentile) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const size_t index = static_cast<size_t>((values.size() - 1) * percentile + 0.5);
        return values[std::min(index, values.size() - 1)];
    }

    void RefreshSnapshot() {
        const size_t count = full_ ? samples_.size() : cursor_;
        frame_scratch_.clear();
        update_scratch_.clear();
        render_scratch_.clear();
        swap_scratch_.clear();
        double total_frame = 0.0;
        for (size_t i = 0; i < count; ++i) {
            if (samples_[i].frame_ms <= 0.0) continue;
            frame_scratch_.push_back(samples_[i].frame_ms);
            update_scratch_.push_back(samples_[i].update_ms);
            render_scratch_.push_back(samples_[i].render_ms);
            swap_scratch_.push_back(samples_[i].swap_ms);
            total_frame += samples_[i].frame_ms;
        }
        snapshot_.fps = total_frame > 0.0 ? frame_scratch_.size() * 1000.0 / total_frame : 0.0;
        snapshot_.frame_p50_ms = Percentile(frame_scratch_, 0.50);
        snapshot_.frame_p95_ms = Percentile(frame_scratch_, 0.95);
        snapshot_.frame_p99_ms = Percentile(frame_scratch_, 0.99);
        snapshot_.update_p95_ms = Percentile(update_scratch_, 0.95);
        snapshot_.render_p95_ms = Percentile(render_scratch_, 0.95);
        snapshot_.swap_p95_ms = Percentile(swap_scratch_, 0.95);
    }
};

#endif
