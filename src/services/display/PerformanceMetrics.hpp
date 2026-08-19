#pragma once

#include <cstddef>
#include <vector>

namespace clockin {

struct PerformanceSnapshot {
    double fps = 0.0;
    double frame_p50_ms = 0.0;
    double frame_p95_ms = 0.0;
    double frame_p99_ms = 0.0;
    double update_p95_ms = 0.0;
    double render_p95_ms = 0.0;
    double swap_p95_ms = 0.0;
    double touch_latency_ms = -1.0;
    int vertices = 0;
    int indices = 0;
    int draw_calls = 0;
};

class PerformanceMetrics {
public:
    explicit PerformanceMetrics(bool enabled);
    bool Enabled() const { return enabled_; }
    void Add(double now_seconds,
             double frame_seconds,
             double update_ms,
             double render_ms,
             double swap_ms,
             double touch_latency_ms,
             int vertices,
             int indices,
             int draw_calls);
    const PerformanceSnapshot& Snapshot() const { return snapshot_; }

private:
    struct Sample {
        double frame_ms = 0.0;
        double update_ms = 0.0;
        double render_ms = 0.0;
        double swap_ms = 0.0;
    };

    bool enabled_;
    std::vector<Sample> samples_;
    std::size_t cursor_ = 0;
    bool full_ = false;
    double last_refresh_ = 0.0;
    PerformanceSnapshot snapshot_;
    std::vector<double> frame_scratch_;
    std::vector<double> update_scratch_;
    std::vector<double> render_scratch_;
    std::vector<double> swap_scratch_;

    static double Percentile(std::vector<double>& values, double percentile);
    void RefreshSnapshot();
};

}  // namespace clockin
