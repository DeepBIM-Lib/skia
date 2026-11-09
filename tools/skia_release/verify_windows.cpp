//
// Created by SherSpidey on 2026/11/10.
//

#include <cstdio>
#include <vector>

#include "include/codec/SkCodec.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkJpegEncoder.h"
#include "include/ports/SkTypeface_win.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "third_party/icu/SkLoadICU.h"
#include "unicode/ubrk.h"

int main(int argc, char** argv)
{
    if (argc != 2 || !SkLoadICU()) return 1;

    auto unicode = SkUnicodes::ICU::Make();
    if (!unicode) return 2;

    UErrorCode error = U_ZERO_ERROR;
    const UChar chinese[] = {0x4e2d, 0x6587, 0x6d4b, 0x8bd5};
    auto breaks = ubrk_open(UBRK_LINE, "zh", chinese, 4, &error);
    if (U_FAILURE(error) || !breaks) return 3;
    const bool has_breaks = ubrk_first(breaks) == 0 && ubrk_next(breaks) > 0;
    ubrk_close(breaks);
    if (!has_breaks) return 4;

    auto manager = SkFontMgr_New_DirectWrite();
    auto face = manager ? manager->makeFromFile(argv[1]) : nullptr;
    if (!face) return 5;

    SkFont font(face, 32);
    const auto glyph = font.unicharToGlyph('A');
    const auto path = font.getPath(glyph);
    if (!glyph || !path || path->isEmpty()) return 6;

    auto provider = sk_make_sp<skia::textlayout::TypefaceFontProvider>();
    provider->registerTypeface(face, SkString("SDKFont"));
    auto fonts = sk_make_sp<skia::textlayout::FontCollection>();
    fonts->setAssetFontManager(provider);
    fonts->disableFontFallback();

    skia::textlayout::TextStyle style;
    style.setFontFamilies({SkString("SDKFont")});
    style.setFontSize(32);
    skia::textlayout::ParagraphStyle paragraph_style;
    paragraph_style.setTextStyle(style);
    auto builder = skia::textlayout::ParagraphBuilder::make(paragraph_style, fonts, unicode);
    builder->addText("Nex 123");
    auto paragraph = builder->Build();
    paragraph->layout(240);
    if (paragraph->unresolvedGlyphs() != 0 || paragraph->getHeight() <= 0) return 7;

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
    if (!surface) return 8;
    surface->getCanvas()->clear(SK_ColorBLUE);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    surface->getCanvas()->drawPath(*path, paint);

    SkPixmap pixels;
    if (!surface->peekPixels(&pixels) || pixels.getColor(63, 63) != SK_ColorBLUE) return 9;

    auto encoded = SkJpegEncoder::Encode(pixels, {});
    auto codec = encoded ? SkCodec::MakeFromData(encoded) : nullptr;
    if (!codec || codec->dimensions() != SkISize::Make(64, 64)) return 10;

    const auto info = codec->getInfo().makeColorType(kRGBA_8888_SkColorType);
    std::vector<unsigned char> decoded(info.computeMinByteSize());
    if (codec->getPixels(info, decoded.data(), info.minRowBytes()) != SkCodec::kSuccess) return 11;

    std::puts("PASS: embedded ICU, Chinese breaks, Unicode, TTF outline, shaping, CPU raster, JPEG roundtrip");
    return 0;
}
