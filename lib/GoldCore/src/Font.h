// Bitmap fonts designed for the 32x8 LED matrix.
//   FONT_4X6 : digits are 4 px wide, 6 px tall (used for <= 5 characters)
//   FONT_3X6 : digits are 3 px wide, 6 px tall (used for 6 characters)
// Glyph rows are stored MSB-left: for a 4 px wide glyph, bit 3 is the
// left-most column; for a 3 px wide glyph, bit 2 is the left-most column.
// FONT_4X6 is slightly proportional: '1' is 3 px wide so the spacing after
// it looks the same as after every other digit.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gold {

struct Glyph {
    char ch;
    uint8_t width;  // 0 = the font's default width (narrow '1' uses 3)
    uint8_t rows[6];
};

struct Font {
    uint8_t width;
    uint8_t height;
    const Glyph* glyphs;
    size_t count;

    const Glyph* find(char c) const;
    int glyphWidth(const Glyph& g) const { return g.width ? g.width : width; }
    bool pixel(const Glyph& g, int x, int y) const {
        const int w = glyphWidth(g);
        if (x < 0 || x >= w || y < 0 || y >= height) return false;
        return (g.rows[y] >> (w - 1 - x)) & 1U;
    }
};

extern const Font FONT_4X6;
extern const Font FONT_3X6;

}  // namespace gold
