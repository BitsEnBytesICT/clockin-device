#pragma once

#include "pages/Page.hpp"
#include "ui/SignaturePad.hpp"

#include <string>

namespace clockin {

class SignaturePage final : public Page {
public:
    PageId Id() const override { return PageId::Signature; }
    UiControl HitTest(const ImVec2& point) const override;
    void Render(ImDrawList& draw, PageDrawing& drawing) const override;
    void Start(std::string user_name);
    bool IsSignatureArea(const ImVec2& point) const;
    void BeginStroke(const ImVec2& point);
    void ContinueStroke(const ImVec2& point);
    void EndStroke();
    void CancelStroke();
    void Clear();
    bool BuildPayload(std::string& svg);
    bool IsDrawing() const { return signature_.IsDrawing(); }
    std::size_t RawSampleCount() const { return signature_.RawSampleCount(); }
    std::size_t PointCount() const { return signature_.TotalPointCount(); }

private:
    std::string user_name_;
    std::string warning_;
    SignaturePad signature_;
};

}  // namespace clockin
