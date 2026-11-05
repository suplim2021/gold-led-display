#include "DisplayRenderer.h"

#include <string.h>

namespace gold {

const Font* fontForLength(int length) {
    if (length <= 0) return nullptr;
    if (length <= 5) return &FONT_4X6;
    if (length == 6) return &FONT_3X6;
    return nullptr;
}

int textWidth(const Font& font, const char* text) {
    int n = (int)strlen(text);
    if (n == 0) return 0;
    return n * font.width + (n - 1) * kGlyphSpacing;
}

bool drawText(Frame& frame, const Font& font, const char* text, int x, int y) {
    const uint16_t clippedBefore = frame.clipped;
    int cx = x;
    for (const char* p = text; *p; ++p) {
        const Glyph* g = font.find(*p);
        if (!g) return false;
        for (int gy = 0; gy < font.height; ++gy) {
            for (int gx = 0; gx < font.width; ++gx) {
                if (font.pixel(*g, gx, gy)) frame.set(cx + gx, y + gy, true);
            }
        }
        // Even blank glyph cells must fit inside the frame.
        if (!Frame::inBounds(cx, y) || !Frame::inBounds(cx + font.width - 1, y + font.height - 1)) {
            ++frame.clipped;
        }
        cx += font.width + kGlyphSpacing;
    }
    return frame.clipped == clippedBefore;
}

RenderResult renderCenteredText(Frame& frame, const char* text) {
    frame.clear();
    RenderResult r = {false, 0, 0};
    const Font* font = fontForLength((int)strlen(text));
    if (!font) return r;
    r.width = textWidth(*font, text);
    if (r.width > Frame::kWidth) return r;
    r.x = (Frame::kWidth - r.width) / 2;
    r.ok = drawText(frame, *font, text, r.x, kGlyphTop);
    return r;
}

int formatPrice(int32_t price, char* buf, int bufSize) {
    if (price < 0 || bufSize < 12) return 0;
    char tmp[12];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (price % 10));
        price /= 10;
    } while (price > 0 && n < 11);
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
    return n;
}

RenderResult renderPrice(Frame& frame, int32_t price, PriceMarker marker, bool staleUnderline) {
    frame.clear();
    RenderResult r = {false, 0, 0};
    if (price < 0 || price > kMaxDisplayablePrice) return r;
    char digits[12];
    if (formatPrice(price, digits, sizeof(digits)) == 0) return r;

    r = renderCenteredText(frame, digits);
    if (!r.ok) return r;

    if (marker == PriceMarker::Sell) {
        frame.set(0, 1);
        frame.set(0, 2);
    } else if (marker == PriceMarker::Buy) {
        frame.set(0, 5);
        frame.set(0, 6);
    }
    if (staleUnderline) {
        for (int x = r.x; x < r.x + r.width; x += 2) frame.set(x, Frame::kHeight - 1);
    }
    r.ok = (frame.clipped == 0);
    return r;
}

}  // namespace gold
