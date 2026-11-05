// Turns prices / status codes into a 32x8 Frame.
//
// Layout rules (see README):
//   * 1..5 characters -> FONT_4X6, 1 px spacing. 5 digits = 24 px, margins 4/4
//     (narrow '1' is 3 px, so prices with 1s are a bit narrower, still centred).
//   * 6 characters    -> FONT_3X6, 1 px spacing. 6 digits = 23 px, margins 4/5.
//   * Glyphs occupy rows 1..6, leaving row 0 and row 7 blank (1 px margin).
//   * Horizontal start x = (32 - width) / 2 (rounded down).
//
// Indicators (only drawn when requested, never on the plain Mode A screen):
//   * Sell marker : column 0, rows 1-2 (top-left)    -> "ขายออก"
//   * Buy marker  : column 0, rows 5-6 (bottom-left) -> "รับซื้อ"
//   * Stale       : dotted line on row 7 under the digits (every other px)
//   * Trend up    : small triangle, columns 29-31, rows 1-2 (top-right)
//   * Trend down  : small triangle, columns 29-31, rows 5-6 (bottom-right)
//     (direction is encoded by both shape and position)
// World price is drawn as a tall "$" (rows 0-6) + digits, e.g. "$4132".
#pragma once

#include <stdint.h>

#include "Font.h"
#include "Frame.h"

namespace gold {

enum class PriceMarker : uint8_t { None, Sell, Buy };

struct RenderResult {
    bool ok;     // false if text is unsupported or would overflow the frame
    int x;       // left column of the text block
    int width;   // width of the text block in pixels
};

constexpr int kGlyphTop = 1;        // first row used by glyphs
constexpr int kGlyphSpacing = 1;    // columns between glyphs
constexpr int32_t kMaxDisplayablePrice = 999999;

// Picks the font for a given character count (nullptr if too long).
const Font* fontForLength(int length);

// Width in px of `text` drawn with `font` and kGlyphSpacing.
int textWidth(const Font& font, const char* text);

// Draws `text` at (x, y). Unknown characters fail the call. Any pixel that
// would fall outside the frame is counted in frame.clipped and fails the call.
bool drawText(Frame& frame, const Font& font, const char* text, int x, int y);

// Clears the frame and draws `text` centred (auto font selection).
RenderResult renderCenteredText(Frame& frame, const char* text);

// Clears the frame and draws a price (0..999999) centred, without commas.
// trend: -1 down, 0 none, +1 up.
RenderResult renderPrice(Frame& frame, int32_t price,
                         PriceMarker marker = PriceMarker::None,
                         bool staleUnderline = false, int8_t trend = 0);

// Clears the frame and draws "$<usd>" centred (world spot price, USD/oz).
RenderResult renderWorldPrice(Frame& frame, int32_t usd, bool staleUnderline = false,
                              int8_t trend = 0);

// Draws only the trend triangle (exposed for tests / preview).
void drawTrend(Frame& frame, int8_t trend);

// Converts a non-negative integer to decimal digits (no separators).
// Returns number of characters written (0 on failure). `buf` >= 12 bytes.
int formatPrice(int32_t price, char* buf, int bufSize);

}  // namespace gold
