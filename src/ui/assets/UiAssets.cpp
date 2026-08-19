#include "ui/assets/UiAssets.hpp"

#include "ui/assets/BrandedIcons.generated.hpp"
#include "ui/assets/OutfitFonts.generated.hpp"

#include <cstddef>
#include <cstdio>

namespace clockin {
namespace {

bool CopyIconMask(ImFontAtlas* atlas,
                  ImFontAtlasRectId rectangle_id,
                  const unsigned char* alpha,
                  unsigned char* pixels,
                  int atlas_width,
                  int bytes_per_pixel) {
    ImFontAtlasRect rectangle;
    if (!atlas->GetCustomRect(rectangle_id, &rectangle) ||
        rectangle.w != kBrandedIconSize || rectangle.h != kBrandedIconSize ||
        bytes_per_pixel != 4) {
        return false;
    }
    for (int y = 0; y < kBrandedIconSize; ++y) {
        for (int x = 0; x < kBrandedIconSize; ++x) {
            unsigned char* destination =
                pixels + ((rectangle.y + y) * atlas_width + rectangle.x + x) * bytes_per_pixel;
            destination[0] = 255;
            destination[1] = 255;
            destination[2] = 255;
            destination[3] = alpha[y * kBrandedIconSize + x];
        }
    }
    return true;
}

void PreloadGlyphs(ImFont* font, float size_pixels) {
    if (font == nullptr) return;
    ImFontBaked* baked = font->GetFontBaked(size_pixels);
    if (baked == nullptr) return;
    for (ImWchar codepoint = 0x0020; codepoint <= 0x017F; ++codepoint) {
        baked->FindGlyphNoFallback(codepoint);
    }
    baked->FindGlyphNoFallback(0x20AC);
}

}  // namespace

FontBundle AddBrandedFonts(ImGuiIO& io, float size_pixels) {
    FontBundle bundle;
    static const ImWchar glyph_ranges[] = {0x0020, 0x017F, 0x20AC, 0x20AC, 0};
    io.Fonts->TexMinWidth = 512;
    io.Fonts->TexMaxWidth = 512;
    io.Fonts->TexMaxHeight = 1024;

    ImFontConfig regular_config;
    regular_config.FontDataOwnedByAtlas = false;
    regular_config.OversampleH = 1;
    regular_config.OversampleV = 1;
    regular_config.PixelSnapH = true;
    std::snprintf(regular_config.Name, sizeof(regular_config.Name), "Outfit Regular");
    bundle.regular = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kOutfitRegularSubset),
        static_cast<int>(kOutfitRegularSubsetSize), size_pixels, &regular_config, glyph_ranges);

    ImFontConfig semibold_config;
    semibold_config.FontDataOwnedByAtlas = false;
    semibold_config.OversampleH = 1;
    semibold_config.OversampleV = 1;
    semibold_config.PixelSnapH = true;
    std::snprintf(semibold_config.Name, sizeof(semibold_config.Name), "Outfit SemiBold");
    bundle.semibold = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kOutfitSemiBoldSubset),
        static_cast<int>(kOutfitSemiBoldSubsetSize), size_pixels, &semibold_config, glyph_ranges);
    if (bundle.regular == nullptr) bundle.regular = io.Fonts->AddFontDefault();
    if (bundle.semibold == nullptr) bundle.semibold = bundle.regular;
    io.FontDefault = bundle.regular;
    return bundle;
}

void InstallBrandedIcons(ImGuiIO& io, FontBundle& bundle, float size_pixels) {
    PreloadGlyphs(bundle.regular, size_pixels);
    if (bundle.semibold != bundle.regular) PreloadGlyphs(bundle.semibold, size_pixels);
    struct IconSource {
        ImWchar codepoint;
        const unsigned char* alpha;
    };
    static const IconSource sources[] = {
        {icons::Settings, kIconSettingsAlpha}, {icons::Delete, kIconDeleteAlpha},
        {icons::Close, kIconCloseAlpha},       {icons::Signature, kIconSignatureAlpha},
        {icons::Back, kIconBackAlpha},         {icons::Check, kIconCheckAlpha},
    };
    ImFontAtlasRectId rectangle_ids[sizeof(sources) / sizeof(sources[0])];
    bool valid = true;
    for (std::size_t index = 0; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        rectangle_ids[index] = io.Fonts->AddCustomRectFontGlyphForSize(
            bundle.semibold, size_pixels, sources[index].codepoint, kBrandedIconSize,
            kBrandedIconSize, 28.0f, ImVec2(0.0f, 0.0f));
        if (rectangle_ids[index] == ImFontAtlasRectId_Invalid) {
            valid = false;
        } else {
            ImFontBaked* baked = bundle.semibold->GetFontBaked(size_pixels);
            ImFontGlyph* glyph = baked == nullptr
                                     ? nullptr
                                     : baked->FindGlyphNoFallback(sources[index].codepoint);
            if (glyph != nullptr) glyph->Colored = false;
        }
    }
    ImTextureData* texture = io.Fonts->TexData;
    unsigned char* pixels = texture == nullptr ? nullptr : texture->Pixels;
    const int bytes_per_pixel = texture == nullptr ? 0 : texture->BytesPerPixel;
    bundle.atlas_width = texture == nullptr ? 0 : texture->Width;
    bundle.atlas_height = texture == nullptr ? 0 : texture->Height;
    bool copied = valid && pixels != nullptr;
    for (std::size_t index = 0; copied && index < sizeof(sources) / sizeof(sources[0]); ++index) {
        copied = CopyIconMask(io.Fonts, rectangle_ids[index], sources[index].alpha, pixels,
                              bundle.atlas_width, bytes_per_pixel);
    }
    bundle.icons_installed = copied;
}

}  // namespace clockin
