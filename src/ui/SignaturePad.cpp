#include "ui/SignaturePad.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace clockin {

SignaturePad::SignaturePad() {
    strokes_.reserve(kMaxStrokes);
    current_.reserve(128);
    raw_window_.reserve(3);
}

void SignaturePad::Clear() {
    strokes_.clear();
    current_.clear();
    raw_window_.clear();
    drawing_ = false;
    too_complex_ = false;
    committed_path_length_ = 0.0f;
    current_path_length_ = 0.0f;
    raw_sample_count_ = 0;
}

void SignaturePad::Begin(const ImVec2& point) {
    if (strokes_.size() >= kMaxStrokes || TotalPointCount() >= kMaxPoints) {
        too_complex_ = true;
        return;
    }
    current_.clear();
    raw_window_.clear();
    const ImVec2 clamped = ClampToArea(point);
    current_.push_back(clamped);
    raw_window_.push_back(clamped);
    ++raw_sample_count_;
    current_path_length_ = 0.0f;
    drawing_ = true;
}

void SignaturePad::Add(const ImVec2& point) {
    if (!drawing_ || too_complex_) return;
    ++raw_sample_count_;
    const ImVec2 clamped = ClampToArea(point);
    if (!raw_window_.empty() && Distance(raw_window_.back(), clamped) < 0.75f) return;
    if (TotalPointCount() >= kMaxPoints) {
        too_complex_ = true;
        return;
    }
    if (!raw_window_.empty()) current_path_length_ += Distance(raw_window_.back(), clamped);
    raw_window_.push_back(clamped);
    if (raw_window_.size() > 3) raw_window_.erase(raw_window_.begin());
    if (raw_window_.size() == 2) {
        current_.push_back(clamped);
        return;
    }
    ImVec2 filtered(raw_window_[0].x * 0.25f + raw_window_[1].x * 0.50f +
                        raw_window_[2].x * 0.25f,
                    raw_window_[0].y * 0.25f + raw_window_[1].y * 0.50f +
                        raw_window_[2].y * 0.25f);
    filtered = ClampDisplacement(filtered, raw_window_[1], 0.5f);
    if (!current_.empty()) current_.back() = filtered;
    current_.push_back(clamped);
}

void SignaturePad::End() {
    if (!drawing_) return;
    drawing_ = false;
    raw_window_.clear();
    if (current_.size() >= 2 && current_path_length_ >= kMinimumStrokeLength) {
        std::vector<ImVec2> simplified;
        Simplify(current_, 0.5f, simplified);
        if (simplified.size() >= 2) {
            strokes_.push_back(std::move(simplified));
            committed_path_length_ += current_path_length_;
        }
    }
    current_.clear();
    current_path_length_ = 0.0f;
    if (TotalPointCount() > kMaxPoints) too_complex_ = true;
}

void SignaturePad::CancelStroke() {
    current_.clear();
    raw_window_.clear();
    drawing_ = false;
    current_path_length_ = 0.0f;
}

bool SignaturePad::IsValid() const {
    return !too_complex_ && TotalPointCount() >= 2 &&
           committed_path_length_ >= kMinimumStrokeLength;
}

float SignaturePad::PathLength() const {
    return committed_path_length_ + current_path_length_;
}

std::size_t SignaturePad::TotalPointCount() const {
    std::size_t count = current_.size();
    for (const auto& stroke : strokes_) count += stroke.size();
    return count;
}

bool SignaturePad::SerializeSvg(std::string& svg) const {
    svg.clear();
    if (!IsValid()) return false;
    std::ostringstream output;
    output << "<svg width=\"550\" height=\"270\" viewBox=\"0 0 550 270\" "
              "xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"550\" height=\"270\" "
              "fill=\"white\"/><g fill=\"none\" stroke=\"black\" stroke-width=\"3\" "
              "stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    for (const auto& stroke : strokes_) {
        if (stroke.size() < 2) continue;
        output << "<path d=\"M";
        for (std::size_t index = 0; index < stroke.size(); ++index) {
            const int x = static_cast<int>(std::floor(stroke[index].x - 49.5f));
            const int y = static_cast<int>(std::floor(stroke[index].y - 149.5f));
            if (index != 0) output << ' ';
            output << std::max(0, std::min(550, x)) << ' '
                   << std::max(0, std::min(270, y));
        }
        output << "\"/>";
    }
    output << "</g></svg>";
    svg = output.str();
    if (svg.size() <= kMaxSvgBytes) return true;
    svg.clear();
    return false;
}

ImVec2 SignaturePad::ClampToArea(const ImVec2& point) {
    return {std::max(50.0f, std::min(600.0f, point.x)),
            std::max(150.0f, std::min(420.0f, point.y))};
}

float SignaturePad::Distance(const ImVec2& first, const ImVec2& second) {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    return std::sqrt(dx * dx + dy * dy);
}

ImVec2 SignaturePad::ClampDisplacement(const ImVec2& value,
                                       const ImVec2& reference,
                                       float maximum) {
    const float dx = value.x - reference.x;
    const float dy = value.y - reference.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= maximum || length <= 0.0001f) return value;
    const float scale = maximum / length;
    return {reference.x + dx * scale, reference.y + dy * scale};
}

float SignaturePad::DistanceToSegment(const ImVec2& point,
                                      const ImVec2& start,
                                      const ImVec2& end) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.0001f) return Distance(point, start);
    float t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared;
    t = std::max(0.0f, std::min(1.0f, t));
    return Distance(point, {start.x + t * dx, start.y + t * dy});
}

void SignaturePad::SimplifyRange(const std::vector<ImVec2>& input,
                                 std::size_t first,
                                 std::size_t last,
                                 float epsilon,
                                 std::vector<bool>& keep) {
    if (last <= first + 1) return;
    float maximum_distance = 0.0f;
    std::size_t maximum_index = first;
    for (std::size_t index = first + 1; index < last; ++index) {
        const float distance = DistanceToSegment(input[index], input[first], input[last]);
        if (distance > maximum_distance) {
            maximum_distance = distance;
            maximum_index = index;
        }
    }
    if (maximum_distance <= epsilon) return;
    keep[maximum_index] = true;
    SimplifyRange(input, first, maximum_index, epsilon, keep);
    SimplifyRange(input, maximum_index, last, epsilon, keep);
}

void SignaturePad::Simplify(const std::vector<ImVec2>& input,
                            float epsilon,
                            std::vector<ImVec2>& output) {
    output.clear();
    if (input.size() <= 2) {
        output = input;
        return;
    }
    std::vector<bool> keep(input.size(), false);
    keep.front() = true;
    keep.back() = true;
    SimplifyRange(input, 0, input.size() - 1, epsilon, keep);
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (keep[index]) output.push_back(input[index]);
    }
}

}  // namespace clockin
