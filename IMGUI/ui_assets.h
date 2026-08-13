#ifndef BITS_BYTES_UI_ASSETS_H
#define BITS_BYTES_UI_ASSETS_H

#include "imgui.h"
#include "assets/fonts/outfit_fonts.h"
#include "assets/ui/branded_icons.h"

#include <cstddef>
#include <cstdio>

struct BrandedFontBundle {
    ImFont* regular;
    ImFont* semibold;
    int atlas_width;
    int atlas_height;
    bool icons_installed;

    BrandedFontBundle()
        : regular(NULL), semibold(NULL), atlas_width(0), atlas_height(0),
          icons_installed(false) {}
};

namespace BrandedIcons {
static const ImWchar Settings = 0xE000;
static const ImWchar Delete = 0xE001;
static const ImWchar Close = 0xE002;
static const ImWchar Signature = 0xE003;
static const ImWchar Back = 0xE004;
static const ImWchar Check = 0xE005;
}

inline bool CopyIconMask(ImFontAtlas* atlas,
                         ImFontAtlasRectId rect_id,
                         const unsigned char* alpha,
                         unsigned char* pixels,
                         int atlas_width,
                         int bytes_per_pixel) {
    ImFontAtlasRect rect;
    if (!atlas->GetCustomRect(rect_id, &rect) || rect.w != kBrandedIconSize ||
        rect.h != kBrandedIconSize || bytes_per_pixel != 4) {
        return false;
    }
    for (int y = 0; y < kBrandedIconSize; ++y) {
        for (int x = 0; x < kBrandedIconSize; ++x) {
            unsigned char* destination = pixels +
                ((rect.y + y) * atlas_width + rect.x + x) * bytes_per_pixel;
            destination[0] = 255;
            destination[1] = 255;
            destination[2] = 255;
            destination[3] = alpha[y * kBrandedIconSize + x];
        }
    }
    return true;
}

inline BrandedFontBundle AddBrandedFonts(ImGuiIO& io, float size_pixels) {
    BrandedFontBundle bundle;
    static const ImWchar glyph_ranges[] = {
        0x0020, 0x017F,
        0x20AC, 0x20AC,
        0,
    };

    io.Fonts->TexMinWidth = 512;
    io.Fonts->TexMaxWidth = 512;
    io.Fonts->TexMaxHeight = 1024;

    ImFontConfig regular_config;
    regular_config.FontDataOwnedByAtlas = false;
    regular_config.OversampleH = 1;
    regular_config.OversampleV = 1;
    regular_config.PixelSnapH = true;
    snprintf(regular_config.Name, sizeof(regular_config.Name), "Outfit Regular");
    bundle.regular = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kOutfitRegularSubset),
        static_cast<int>(kOutfitRegularSubsetSize),
        size_pixels,
        &regular_config,
        glyph_ranges);

    ImFontConfig semibold_config;
    semibold_config.FontDataOwnedByAtlas = false;
    semibold_config.OversampleH = 1;
    semibold_config.OversampleV = 1;
    semibold_config.PixelSnapH = true;
    snprintf(semibold_config.Name, sizeof(semibold_config.Name), "Outfit SemiBold");
    bundle.semibold = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kOutfitSemiBoldSubset),
        static_cast<int>(kOutfitSemiBoldSubsetSize),
        size_pixels,
        &semibold_config,
        glyph_ranges);

    if (bundle.regular == NULL) bundle.regular = io.Fonts->AddFontDefault();
    if (bundle.semibold == NULL) bundle.semibold = bundle.regular;
    io.FontDefault = bundle.regular;
    return bundle;
}

inline void PreloadBrandedGlyphs(ImFont* font, float size_pixels) {
    if (font == NULL) return;
    ImFontBaked* baked = font->GetFontBaked(size_pixels);
    if (baked == NULL) return;
    for (ImWchar codepoint = 0x0020; codepoint <= 0x017F; ++codepoint) {
        baked->FindGlyphNoFallback(codepoint);
    }
    baked->FindGlyphNoFallback(0x20AC);
}

inline void InstallBrandedIcons(ImGuiIO& io,
                                BrandedFontBundle& bundle,
                                float size_pixels) {
    PreloadBrandedGlyphs(bundle.regular, size_pixels);
    if (bundle.semibold != bundle.regular) {
        PreloadBrandedGlyphs(bundle.semibold, size_pixels);
    }

    struct IconSource {
        ImWchar codepoint;
        const unsigned char* alpha;
    };
    static const IconSource icons[] = {
        {BrandedIcons::Settings, kIconSettingsAlpha},
        {BrandedIcons::Delete, kIconDeleteAlpha},
        {BrandedIcons::Close, kIconCloseAlpha},
        {BrandedIcons::Signature, kIconSignatureAlpha},
        {BrandedIcons::Back, kIconBackAlpha},
        {BrandedIcons::Check, kIconCheckAlpha},
    };
    ImFontAtlasRectId rect_ids[sizeof(icons) / sizeof(icons[0])];
    bool rects_valid = true;
    for (size_t index = 0; index < sizeof(icons) / sizeof(icons[0]); ++index) {
        rect_ids[index] = io.Fonts->AddCustomRectFontGlyphForSize(
            bundle.semibold,
            size_pixels,
            icons[index].codepoint,
            kBrandedIconSize,
            kBrandedIconSize,
            28.0f,
            ImVec2(0.0f, 0.0f));
        if (rect_ids[index] == ImFontAtlasRectId_Invalid) {
            rects_valid = false;
        } else {
            ImFontBaked* baked = bundle.semibold->GetFontBaked(size_pixels);
            ImFontGlyph* glyph = baked != NULL
                ? baked->FindGlyphNoFallback(icons[index].codepoint)
                : NULL;
            if (glyph != NULL) glyph->Colored = false;
        }
    }

    ImTextureData* texture = io.Fonts->TexData;
    unsigned char* pixels = texture != NULL ? texture->Pixels : NULL;
    const int bytes_per_pixel = texture != NULL ? texture->BytesPerPixel : 0;
    bundle.atlas_width = texture != NULL ? texture->Width : 0;
    bundle.atlas_height = texture != NULL ? texture->Height : 0;
    bool copied = rects_valid && pixels != NULL;
    for (size_t index = 0; copied && index < sizeof(icons) / sizeof(icons[0]); ++index) {
        copied = CopyIconMask(io.Fonts,
                              rect_ids[index],
                              icons[index].alpha,
                              pixels,
                              bundle.atlas_width,
                              bytes_per_pixel);
    }
    bundle.icons_installed = copied;
}

#endif
