#include "services/display/PerformanceMetrics.hpp"

#include <algorithm>

namespace clockin {

PerformanceMetrics::PerformanceMetrics(bool enabled) : enabled_(enabled), samples_(300) {
    frame_scratch_.reserve(samples_.size());
    update_scratch_.reserve(samples_.size());
    render_scratch_.reserve(samples_.size());
    swap_scratch_.reserve(samples_.size());
}

void PerformanceMetrics::Add(double now_seconds,
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

double PerformanceMetrics::Percentile(std::vector<double>& values, double percentile) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>((values.size() - 1) * percentile + 0.5);
    return values[std::min(index, values.size() - 1)];
}

void PerformanceMetrics::RefreshSnapshot() {
    const std::size_t count = full_ ? samples_.size() : cursor_;
    frame_scratch_.clear();
    update_scratch_.clear();
    render_scratch_.clear();
    swap_scratch_.clear();
    double total_frame = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (samples_[index].frame_ms <= 0.0) continue;
        frame_scratch_.push_back(samples_[index].frame_ms);
        update_scratch_.push_back(samples_[index].update_ms);
        render_scratch_.push_back(samples_[index].render_ms);
        swap_scratch_.push_back(samples_[index].swap_ms);
        total_frame += samples_[index].frame_ms;
    }
    snapshot_.fps = total_frame > 0.0 ? frame_scratch_.size() * 1000.0 / total_frame : 0.0;
    snapshot_.frame_p50_ms = Percentile(frame_scratch_, 0.50);
    snapshot_.frame_p95_ms = Percentile(frame_scratch_, 0.95);
    snapshot_.frame_p99_ms = Percentile(frame_scratch_, 0.99);
    snapshot_.update_p95_ms = Percentile(update_scratch_, 0.95);
    snapshot_.render_p95_ms = Percentile(render_scratch_, 0.95);
    snapshot_.swap_p95_ms = Percentile(swap_scratch_, 0.95);
}

}  // namespace clockin
