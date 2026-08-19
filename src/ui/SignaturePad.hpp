#pragma once

#include "imgui.h"

#include <cstddef>
#include <string>
#include <vector>

namespace clockin {

class SignaturePad {
public:
    static constexpr std::size_t kMaxStrokes = 64;
    static constexpr std::size_t kMaxPoints = 1024;
    static constexpr std::size_t kMaxSvgBytes = 12 * 1024;

    SignaturePad();

    void Clear();
    void Begin(const ImVec2& point);
    void Add(const ImVec2& point);
    void End();
    void CancelStroke();

    bool IsDrawing() const { return drawing_; }
    bool TooComplex() const { return too_complex_; }
    bool IsValid() const;
    float PathLength() const;
    std::size_t TotalPointCount() const;
    std::size_t RawSampleCount() const { return raw_sample_count_; }
    const std::vector<std::vector<ImVec2>>& Strokes() const { return strokes_; }
    const std::vector<ImVec2>& CurrentStroke() const { return current_; }
    bool SerializeSvg(std::string& svg) const;

private:
    static constexpr float kMinimumStrokeLength = 5.0f;

    std::vector<std::vector<ImVec2>> strokes_;
    std::vector<ImVec2> current_;
    std::vector<ImVec2> raw_window_;
    bool drawing_ = false;
    bool too_complex_ = false;
    float committed_path_length_ = 0.0f;
    float current_path_length_ = 0.0f;
    std::size_t raw_sample_count_ = 0;

    static ImVec2 ClampToArea(const ImVec2& point);
    static float Distance(const ImVec2& first, const ImVec2& second);
    static ImVec2 ClampDisplacement(const ImVec2& value,
                                    const ImVec2& reference,
                                    float maximum);
    static float DistanceToSegment(const ImVec2& point,
                                   const ImVec2& start,
                                   const ImVec2& end);
    static void SimplifyRange(const std::vector<ImVec2>& input,
                              std::size_t first,
                              std::size_t last,
                              float epsilon,
                              std::vector<bool>& keep);
    static void Simplify(const std::vector<ImVec2>& input,
                         float epsilon,
                         std::vector<ImVec2>& output);
};

}  // namespace clockin
