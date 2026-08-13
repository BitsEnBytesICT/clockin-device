#ifndef SIGNATURE_PAD_H
#define SIGNATURE_PAD_H

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

class SignaturePad {
public:
    static const size_t MAX_STROKES = 64;
    static const size_t MAX_POINTS = 1024;
    static const size_t MAX_SVG_BYTES = 12 * 1024;

    SignaturePad()
        : drawing_(false), too_complex_(false), committed_path_length_(0.0f),
          current_path_length_(0.0f), raw_sample_count_(0) {
        strokes_.reserve(MAX_STROKES);
        current_.reserve(128);
        raw_window_.reserve(3);
    }

    void Clear() {
        strokes_.clear();
        current_.clear();
        raw_window_.clear();
        drawing_ = false;
        too_complex_ = false;
        committed_path_length_ = 0.0f;
        current_path_length_ = 0.0f;
        raw_sample_count_ = 0;
    }

    void Begin(const ImVec2& point) {
        if (strokes_.size() >= MAX_STROKES || TotalPointCount() >= MAX_POINTS) {
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

    void Add(const ImVec2& point) {
        if (!drawing_ || too_complex_) return;
        ++raw_sample_count_;
        const ImVec2 clamped = ClampToArea(point);
        if (!raw_window_.empty() && Distance(raw_window_.back(), clamped) < 0.75f) return;
        if (TotalPointCount() >= MAX_POINTS) {
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

        // Replace the prior raw preview point with a very lightly filtered
        // point, then append the newest raw point as a zero-lag visible tip.
        ImVec2 filtered(
            raw_window_[0].x * 0.25f + raw_window_[1].x * 0.50f + raw_window_[2].x * 0.25f,
            raw_window_[0].y * 0.25f + raw_window_[1].y * 0.50f + raw_window_[2].y * 0.25f);
        filtered = ClampDisplacement(filtered, raw_window_[1], 0.5f);
        if (!current_.empty()) current_.back() = filtered;
        current_.push_back(clamped);
    }

    void End() {
        if (!drawing_) return;
        drawing_ = false;
        raw_window_.clear();
        // A tap or very short mark is not a signature. Discard it instead of
        // allowing several tiny marks to add up to the five-pixel threshold.
        if (current_.size() >= 2 && current_path_length_ >= MIN_STROKE_LENGTH) {
            std::vector<ImVec2> simplified;
            Simplify(current_, 0.5f, simplified);
            if (simplified.size() >= 2) {
                strokes_.push_back(std::move(simplified));
                committed_path_length_ += current_path_length_;
            }
        }
        current_.clear();
        current_path_length_ = 0.0f;
        if (TotalPointCount() > MAX_POINTS) too_complex_ = true;
    }

    void CancelStroke() {
        current_.clear();
        raw_window_.clear();
        drawing_ = false;
        current_path_length_ = 0.0f;
    }

    bool IsDrawing() const { return drawing_; }
    bool TooComplex() const { return too_complex_; }
    bool IsValid() const { return !too_complex_ && TotalPointCount() >= 2 && committed_path_length_ >= 5.0f; }
    float PathLength() const { return committed_path_length_ + current_path_length_; }

    size_t TotalPointCount() const {
        size_t count = current_.size();
        for (size_t i = 0; i < strokes_.size(); ++i) count += strokes_[i].size();
        return count;
    }
    size_t RawSampleCount() const { return raw_sample_count_; }

    const std::vector<std::vector<ImVec2>>& Strokes() const { return strokes_; }
    const std::vector<ImVec2>& CurrentStroke() const { return current_; }

    bool SerializeSVG(std::string& svg) const {
        svg.clear();
        if (!IsValid()) return false;
        std::ostringstream output;
        output << "<svg width=\"550\" height=\"270\" viewBox=\"0 0 550 270\" xmlns=\"http://www.w3.org/2000/svg\">"
               << "<rect width=\"550\" height=\"270\" fill=\"white\"/>"
               << "<g fill=\"none\" stroke=\"black\" stroke-width=\"3\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
        for (size_t stroke_index = 0; stroke_index < strokes_.size(); ++stroke_index) {
            const std::vector<ImVec2>& stroke = strokes_[stroke_index];
            if (stroke.size() < 2) continue;
            output << "<path d=\"M";
            for (size_t point_index = 0; point_index < stroke.size(); ++point_index) {
                const int x = static_cast<int>(std::floor(stroke[point_index].x - 50.0f + 0.5f));
                const int y = static_cast<int>(std::floor(stroke[point_index].y - 150.0f + 0.5f));
                if (point_index != 0) output << ' ';
                output << std::max(0, std::min(550, x)) << ' ' << std::max(0, std::min(270, y));
            }
            output << "\"/>";
        }
        output << "</g></svg>";
        svg = output.str();
        if (svg.size() > MAX_SVG_BYTES) {
            svg.clear();
            return false;
        }
        return true;
    }

private:
    static constexpr float MIN_STROKE_LENGTH = 5.0f;

    std::vector<std::vector<ImVec2>> strokes_;
    std::vector<ImVec2> current_;
    std::vector<ImVec2> raw_window_;
    bool drawing_;
    bool too_complex_;
    float committed_path_length_;
    float current_path_length_;
    size_t raw_sample_count_;

    static ImVec2 ClampToArea(const ImVec2& point) {
        return ImVec2(
            std::max(50.0f, std::min(600.0f, point.x)),
            std::max(150.0f, std::min(420.0f, point.y)));
    }

    static float Distance(const ImVec2& a, const ImVec2& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static ImVec2 ClampDisplacement(const ImVec2& value, const ImVec2& reference, float maximum) {
        const float dx = value.x - reference.x;
        const float dy = value.y - reference.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= maximum || length <= 0.0001f) return value;
        const float scale = maximum / length;
        return ImVec2(reference.x + dx * scale, reference.y + dy * scale);
    }

    static float DistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end) {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length_squared = dx * dx + dy * dy;
        if (length_squared <= 0.0001f) return Distance(point, start);
        float t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared;
        t = std::max(0.0f, std::min(1.0f, t));
        return Distance(point, ImVec2(start.x + t * dx, start.y + t * dy));
    }

    static void SimplifyRange(const std::vector<ImVec2>& input,
                              size_t first,
                              size_t last,
                              float epsilon,
                              std::vector<bool>& keep) {
        if (last <= first + 1) return;
        float maximum_distance = 0.0f;
        size_t maximum_index = first;
        for (size_t i = first + 1; i < last; ++i) {
            const float distance = DistanceToSegment(input[i], input[first], input[last]);
            if (distance > maximum_distance) {
                maximum_distance = distance;
                maximum_index = i;
            }
        }
        if (maximum_distance > epsilon) {
            keep[maximum_index] = true;
            SimplifyRange(input, first, maximum_index, epsilon, keep);
            SimplifyRange(input, maximum_index, last, epsilon, keep);
        }
    }

    static void Simplify(const std::vector<ImVec2>& input, float epsilon, std::vector<ImVec2>& output) {
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
        for (size_t i = 0; i < input.size(); ++i) {
            if (keep[i]) output.push_back(input[i]);
        }
    }
};

#endif
