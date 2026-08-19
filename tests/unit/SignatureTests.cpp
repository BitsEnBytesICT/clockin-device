#include "TestHarness.hpp"

#include "ui/SignaturePad.hpp"

TEST_CASE("signature is bounded and serializes to SVG") {
    clockin::SignaturePad signature;
    signature.Begin({60.0f, 200.0f});
    for (int point = 1; point <= 100; ++point) {
        signature.Add({60.0f + point * 3.0f, 200.0f + (point % 4)});
    }
    signature.End();
    std::string svg;
    CHECK(signature.IsValid());
    CHECK(signature.TotalPointCount() <= clockin::SignaturePad::kMaxPoints);
    CHECK(signature.SerializeSvg(svg));
    CHECK(svg.find("<svg") == 0);
    CHECK(svg.size() <= clockin::SignaturePad::kMaxSvgBytes);
}

TEST_CASE("short and cancelled signatures are rejected") {
    clockin::SignaturePad signature;
    signature.Begin({60.0f, 200.0f});
    signature.Add({61.0f, 200.0f});
    signature.End();
    std::string svg;
    CHECK(!signature.SerializeSvg(svg));
    signature.Begin({60.0f, 200.0f});
    signature.Add({200.0f, 220.0f});
    signature.CancelStroke();
    CHECK(!signature.IsValid());
}
