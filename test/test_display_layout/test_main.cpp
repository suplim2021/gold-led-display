// Display layout / font tests (run on PC: pio test -e native)
#include <string.h>
#include <unity.h>

#include <string>

#include "DisplayRenderer.h"
#include "Font.h"
#include "Frame.h"

using namespace gold;

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- helpers --

static int firstLitColumn(const Frame& f) {
    for (int x = 0; x < Frame::kWidth; ++x)
        for (int y = 0; y < Frame::kHeight; ++y)
            if (f.get(x, y)) return x;
    return -1;
}
static int lastLitColumn(const Frame& f) {
    for (int x = Frame::kWidth - 1; x >= 0; --x)
        for (int y = 0; y < Frame::kHeight; ++y)
            if (f.get(x, y)) return x;
    return -1;
}
static bool rowEmpty(const Frame& f, int y) { return f.rows[y] == 0; }

static std::string glyphArt(const Font& font, char c) {
    const Glyph* g = font.find(c);
    std::string s;
    if (!g) return s;
    for (int y = 0; y < font.height; ++y) {
        for (int x = 0; x < font.glyphWidth(*g); ++x) s += font.pixel(*g, x, y) ? '#' : '.';
        s += '\n';
    }
    return s;
}

static std::string frameArt(const Frame& f) {
    std::string s;
    for (int y = 0; y < Frame::kHeight; ++y) {
        for (int x = 0; x < Frame::kWidth; ++x) s += f.get(x, y) ? '#' : '.';
        s += '\n';
    }
    return s;
}

// ------------------------------------------------------- font accuracy ----

static const char* kExpected4x6[10] = {
    ".##.\n#..#\n#..#\n#..#\n#..#\n.##.\n",  // 0
    ".#.\n##.\n.#.\n.#.\n.#.\n###\n",        // 1 (narrow, 3 px)
    ".##.\n#..#\n...#\n..#.\n.#..\n####\n",  // 2
    "###.\n...#\n.##.\n...#\n...#\n###.\n",  // 3
    "#..#\n#..#\n#..#\n####\n...#\n...#\n",  // 4
    "####\n#...\n###.\n...#\n...#\n###.\n",  // 5
    ".##.\n#...\n###.\n#..#\n#..#\n.##.\n",  // 6
    "####\n...#\n..#.\n..#.\n.#..\n.#..\n",  // 7
    ".##.\n#..#\n.##.\n#..#\n#..#\n.##.\n",  // 8
    ".##.\n#..#\n#..#\n.###\n...#\n.##.\n",  // 9
};

static const char* kExpected3x6[10] = {
    "###\n#.#\n#.#\n#.#\n#.#\n###\n",  // 0
    ".#.\n##.\n.#.\n.#.\n.#.\n###\n",  // 1
    "###\n..#\n..#\n###\n#..\n###\n",  // 2
    "###\n..#\n###\n..#\n..#\n###\n",  // 3
    "#.#\n#.#\n#.#\n###\n..#\n..#\n",  // 4
    "###\n#..\n###\n..#\n..#\n###\n",  // 5
    "###\n#..\n###\n#.#\n#.#\n###\n",  // 6
    "###\n..#\n..#\n.#.\n.#.\n.#.\n",  // 7
    "###\n#.#\n###\n#.#\n#.#\n###\n",  // 8
    "###\n#.#\n#.#\n###\n..#\n###\n",  // 9
};

void test_font_4x6_all_digits_match_design() {
    TEST_ASSERT_EQUAL(4, FONT_4X6.width);
    TEST_ASSERT_EQUAL(6, FONT_4X6.height);
    for (int d = 0; d <= 9; ++d) {
        const std::string art = glyphArt(FONT_4X6, (char)('0' + d));
        TEST_ASSERT_EQUAL_STRING_MESSAGE(kExpected4x6[d], art.c_str(), "4x6 digit mismatch");
    }
}

void test_font_3x6_all_digits_match_design() {
    TEST_ASSERT_EQUAL(3, FONT_3X6.width);
    TEST_ASSERT_EQUAL(6, FONT_3X6.height);
    for (int d = 0; d <= 9; ++d) {
        const std::string art = glyphArt(FONT_3X6, (char)('0' + d));
        TEST_ASSERT_EQUAL_STRING_MESSAGE(kExpected3x6[d], art.c_str(), "3x6 digit mismatch");
    }
}

void test_fonts_have_no_unused_bits() {
    const Font* fonts[] = {&FONT_4X6, &FONT_3X6};
    for (const Font* f : fonts) {
        for (size_t i = 0; i < f->count; ++i)
            for (int y = 0; y < 6; ++y)
                TEST_ASSERT_EQUAL_MESSAGE(0, f->glyphs[i].rows[y] >> f->glyphWidth(f->glyphs[i]),
                                          "glyph has bits outside its width");
    }
}

void test_digits_are_distinct() {
    const Font* fonts[] = {&FONT_4X6, &FONT_3X6};
    for (const Font* f : fonts)
        for (int a = 0; a <= 9; ++a)
            for (int b = a + 1; b <= 9; ++b)
                TEST_ASSERT_TRUE(glyphArt(*f, (char)('0' + a)) != glyphArt(*f, (char)('0' + b)));
}

// --------------------------------------------------------------- layout ----

void test_narrow_one_keeps_spacing_even() {
    // 71150: 7(4) 1(3) 1(3) 5(4) 0(4) + 4 gaps = 22 px, centred at x=5
    Frame f;
    RenderResult r = renderPrice(f, 71150);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(22, r.width);
    TEST_ASSERT_EQUAL(5, r.x);
    // every glyph is followed by exactly one empty column
    const int gapCols[] = {9, 13, 17, 22};
    for (int c : gapCols)
        for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(f.get(c, y));
    TEST_ASSERT_EQUAL(0, f.clipped);
}

void test_five_digits_layout() {
    Frame f;
    RenderResult r = renderPrice(f, 65950);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(24, r.width);
    TEST_ASSERT_EQUAL(4, r.x);  // left margin 4
    TEST_ASSERT_EQUAL(0, f.clipped);
    // right margin 4: columns 28..31 empty
    for (int x = 28; x < 32; ++x)
        for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(f.get(x, y));
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(f.get(x, y));
    TEST_ASSERT_TRUE(rowEmpty(f, 0));
    TEST_ASSERT_TRUE(rowEmpty(f, 7));
}

void test_five_digits_exact_pixels() {
    Frame f;
    renderPrice(f, 65950);
    // Build the expectation from the glyph table independently of the renderer.
    std::string want(8 * 33, '.');
    for (int y = 0; y < 8; ++y) want[y * 33 + 32] = '\n';
    const char* digits = "65950";
    for (int i = 0; i < 5; ++i) {
        const int x0 = 4 + i * 5;
        const char* art = kExpected4x6[digits[i] - '0'];
        for (int gy = 0; gy < 6; ++gy)
            for (int gx = 0; gx < 4; ++gx)
                if (art[gy * 5 + gx] == '#') want[(gy + 1) * 33 + x0 + gx] = '#';
    }
    TEST_ASSERT_EQUAL_STRING(want.c_str(), frameArt(f).c_str());
}

void test_six_digits_layout() {
    Frame f;
    RenderResult r = renderPrice(f, 100000);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(23, r.width);
    TEST_ASSERT_EQUAL(4, r.x);                     // left margin 4
    TEST_ASSERT_EQUAL(5, 32 - (r.x + r.width));    // right margin 5 (closest possible)
    TEST_ASSERT_EQUAL(4, firstLitColumn(f));
    TEST_ASSERT_TRUE(rowEmpty(f, 0));
    TEST_ASSERT_TRUE(rowEmpty(f, 7));
    TEST_ASSERT_EQUAL(0, f.clipped);
    // glyph '1' (3x6) at x=4: ".#." on first glyph row -> pixel (5,1)
    TEST_ASSERT_TRUE(f.get(5, 1));
    TEST_ASSERT_FALSE(f.get(4, 1));
}

void test_six_digits_max_value() {
    Frame f;
    RenderResult r = renderPrice(f, 999999);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(4, firstLitColumn(f));
    TEST_ASSERT_EQUAL(26, lastLitColumn(f));
}

void test_horizontal_margins_balanced() {
    const int32_t samples[] = {7, 42, 999, 1234, 65950, 70550, 100000, 123456};
    for (int32_t p : samples) {
        Frame f;
        RenderResult r = renderPrice(f, p);
        TEST_ASSERT_TRUE(r.ok);
        const int left = r.x;
        const int right = Frame::kWidth - (r.x + r.width);
        TEST_ASSERT_TRUE_MESSAGE(left >= 0 && right >= 0, "negative margin");
        TEST_ASSERT_TRUE_MESSAGE(right - left == 0 || right - left == 1, "margins not balanced");
    }
}

void test_vertical_margins_every_digit() {
    for (int d = 0; d <= 9; ++d) {
        Frame f5, f6;
        renderPrice(f5, d * 11111);
        renderPrice(f6, d == 0 ? 100000 : d * 111111);
        TEST_ASSERT_TRUE(rowEmpty(f5, 0));
        TEST_ASSERT_TRUE(rowEmpty(f5, 7));
        TEST_ASSERT_TRUE(rowEmpty(f6, 0));
        TEST_ASSERT_TRUE(rowEmpty(f6, 7));
    }
}

void test_no_comma_in_output() {
    char buf[12];
    TEST_ASSERT_EQUAL(6, formatPrice(100000, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("100000", buf);
    TEST_ASSERT_EQUAL(5, formatPrice(65950, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("65950", buf);
    TEST_ASSERT_NULL(strchr(buf, ','));
}

void test_overflow_rejected_seven_digits() {
    Frame f;
    RenderResult r = renderPrice(f, 1000000);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(0, f.litCount());  // nothing half-drawn
    TEST_ASSERT_FALSE(renderPrice(f, -1).ok);
}

void test_overflow_detection_counts_clipped_pixels() {
    Frame f;
    TEST_ASSERT_FALSE(drawText(f, FONT_4X6, "88888", 10, 1));  // runs off right edge
    TEST_ASSERT_TRUE(f.clipped > 0);
    Frame g;
    TEST_ASSERT_FALSE(drawText(g, FONT_4X6, "8", 0, 3));  // runs off bottom
    TEST_ASSERT_TRUE(g.clipped > 0);
    Frame h;
    TEST_ASSERT_FALSE(drawText(h, FONT_4X6, "1", -2, 1));  // blank cell off left edge
    TEST_ASSERT_TRUE(h.clipped > 0);
}

void test_unknown_character_rejected() {
    Frame f;
    TEST_ASSERT_FALSE(renderCenteredText(f, "6,950").ok);
}

void test_all_six_digit_prices_fit_scan() {
    // Exhaustive-ish scan: every 5/6 digit value step 997 renders without clipping.
    for (int32_t p = 10000; p <= 999999; p += 997) {
        Frame f;
        RenderResult r = renderPrice(f, p);
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL(0, f.clipped);
        TEST_ASSERT_TRUE(rowEmpty(f, 0));
        TEST_ASSERT_TRUE(rowEmpty(f, 7));
        TEST_ASSERT_TRUE(firstLitColumn(f) >= 4);
        TEST_ASSERT_TRUE(lastLitColumn(f) <= 27);
    }
}

// ----------------------------------------------------------- indicators ----

void test_sell_and_buy_markers() {
    Frame s, b;
    renderPrice(s, 65950, PriceMarker::Sell);
    renderPrice(b, 65750, PriceMarker::Buy);
    TEST_ASSERT_TRUE(s.get(0, 1) && s.get(0, 2));
    TEST_ASSERT_FALSE(s.get(0, 5) || s.get(0, 6));
    TEST_ASSERT_TRUE(b.get(0, 5) && b.get(0, 6));
    TEST_ASSERT_FALSE(b.get(0, 1) || b.get(0, 2));
    // marker keeps a gap of at least 3 empty columns to the digits
    for (int x = 1; x < 4; ++x)
        for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(s.get(x, y));
}

void test_stale_underline_only_on_row7_under_digits() {
    Frame clean, stale;
    RenderResult r = renderPrice(clean, 65950);
    renderPrice(stale, 65950, PriceMarker::None, true);
    for (int y = 0; y < 7; ++y) TEST_ASSERT_EQUAL_UINT32(clean.rows[y], stale.rows[y]);
    TEST_ASSERT_EQUAL_UINT32(0, clean.rows[7]);
    int dots = 0;
    for (int x = 0; x < 32; ++x) {
        if (stale.get(x, 7)) {
            ++dots;
            TEST_ASSERT_TRUE(x >= r.x && x < r.x + r.width);
            TEST_ASSERT_FALSE(stale.get(x + 1, 7));  // dotted, not solid
        }
    }
    TEST_ASSERT_EQUAL(12, dots);
}

void test_mode_a_has_no_indicator_pixels() {
    Frame f;
    renderPrice(f, 65950);
    TEST_ASSERT_EQUAL_UINT32(0, f.rows[0]);
    TEST_ASSERT_EQUAL_UINT32(0, f.rows[7]);
    TEST_ASSERT_FALSE(f.get(0, 1));
}

void test_status_codes_render() {
    const char* codes[] = {"E1", "E2", "E3", "E4", "----", "------"};
    for (const char* c : codes) {
        Frame f;
        RenderResult r = renderCenteredText(f, c);
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_TRUE(f.litCount() > 0);
        TEST_ASSERT_TRUE(rowEmpty(f, 0));
        TEST_ASSERT_TRUE(rowEmpty(f, 7));
    }
}

void test_trend_arrows() {
    Frame up, down, none;
    renderPrice(up, 65950, PriceMarker::None, false, 1);
    renderPrice(down, 65950, PriceMarker::None, false, -1);
    renderPrice(none, 65950, PriceMarker::None, false, 0);
    // up: top-right triangle, rows 1-2
    TEST_ASSERT_TRUE(up.get(30, 1) && up.get(29, 2) && up.get(30, 2) && up.get(31, 2));
    TEST_ASSERT_FALSE(up.get(29, 1) || up.get(31, 1));
    for (int y = 3; y < 8; ++y)
        for (int x = 28; x < 32; ++x) TEST_ASSERT_FALSE(up.get(x, y));
    // down: bottom-right triangle, rows 5-6
    TEST_ASSERT_TRUE(down.get(29, 5) && down.get(30, 5) && down.get(31, 5) && down.get(30, 6));
    for (int y = 0; y < 5; ++y)
        for (int x = 28; x < 32; ++x) TEST_ASSERT_FALSE(down.get(x, y));
    // the arrow never touches the digits: column 28 stays empty, row 0/7 stay empty
    for (int y = 0; y < 8; ++y) {
        TEST_ASSERT_FALSE(up.get(28, y));
        TEST_ASSERT_FALSE(down.get(28, y));
    }
    TEST_ASSERT_EQUAL_UINT32(0, up.rows[0] | up.rows[7] | down.rows[0] | down.rows[7]);
    // digits identical with or without arrow
    for (int y = 0; y < 8; ++y) {
        TEST_ASSERT_EQUAL_UINT32(none.rows[y] & 0x0FFFFFFFu, up.rows[y] & 0x0FFFFFFFu);
        TEST_ASSERT_EQUAL_UINT32(none.rows[y] & 0x0FFFFFFFu, down.rows[y] & 0x0FFFFFFFu);
    }
}

void test_arrow_fits_with_six_digits_and_markers() {
    Frame f;
    RenderResult r = renderPrice(f, 100000, PriceMarker::Buy, true, -1);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(0, f.clipped);
    TEST_ASSERT_TRUE(f.get(0, 5) && f.get(30, 6));
    TEST_ASSERT_FALSE(f.get(27, 1) || f.get(28, 1));  // gap between digits and arrow
}

void test_world_price_layout() {
    Frame f;
    RenderResult r = renderWorldPrice(f, 4038, false, 1);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(24, r.width);  // $ (3) + gap 2 + "4038" (19)
    TEST_ASSERT_EQUAL(4, r.x);
    TEST_ASSERT_EQUAL(0, f.clipped);
    // tall "$" at columns 4-6, rows 0-6: stems above and below the S
    const char* dollar[7] = {".#.", "###", "#..", "###", "..#", "###", ".#."};
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 3; ++x) TEST_ASSERT_EQUAL(dollar[y][x] == '#', f.get(4 + x, y));
    // only the $ stem uses row 0; row 7 stays free for the stale underline
    TEST_ASSERT_EQUAL_UINT32(1u << 5, f.rows[0]);
    TEST_ASSERT_TRUE(rowEmpty(f, 7));
    // gap of 2 empty columns between $ and digits
    for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(f.get(7, y) || f.get(8, y));
    TEST_ASSERT_TRUE(f.get(30, 1));       // up arrow present
    TEST_ASSERT_TRUE(f.get(27, 2));       // digits end at column 27 ('8')
    for (int y = 0; y < 8; ++y) TEST_ASSERT_FALSE(f.get(28, y));
    Frame g;
    TEST_ASSERT_TRUE(renderWorldPrice(g, 10250).ok);  // "$10250" -> 3x6 font
    TEST_ASSERT_EQUAL(0, g.clipped);
    Frame h;
    TEST_ASSERT_FALSE(renderWorldPrice(h, 100000).ok);
}

void test_dollar_and_letters_exist() {
    const char* chars = "AP";
    for (const char* c = chars; *c; ++c) {
        TEST_ASSERT_NOT_NULL(FONT_4X6.find(*c));
        TEST_ASSERT_NOT_NULL(FONT_3X6.find(*c));
    }
    Frame f;
    TEST_ASSERT_TRUE(renderCenteredText(f, "AP").ok);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_font_4x6_all_digits_match_design);
    RUN_TEST(test_font_3x6_all_digits_match_design);
    RUN_TEST(test_fonts_have_no_unused_bits);
    RUN_TEST(test_digits_are_distinct);
    RUN_TEST(test_narrow_one_keeps_spacing_even);
    RUN_TEST(test_five_digits_layout);
    RUN_TEST(test_five_digits_exact_pixels);
    RUN_TEST(test_six_digits_layout);
    RUN_TEST(test_six_digits_max_value);
    RUN_TEST(test_horizontal_margins_balanced);
    RUN_TEST(test_vertical_margins_every_digit);
    RUN_TEST(test_no_comma_in_output);
    RUN_TEST(test_overflow_rejected_seven_digits);
    RUN_TEST(test_overflow_detection_counts_clipped_pixels);
    RUN_TEST(test_unknown_character_rejected);
    RUN_TEST(test_all_six_digit_prices_fit_scan);
    RUN_TEST(test_sell_and_buy_markers);
    RUN_TEST(test_stale_underline_only_on_row7_under_digits);
    RUN_TEST(test_mode_a_has_no_indicator_pixels);
    RUN_TEST(test_status_codes_render);
    RUN_TEST(test_trend_arrows);
    RUN_TEST(test_arrow_fits_with_six_digits_and_markers);
    RUN_TEST(test_world_price_layout);
    RUN_TEST(test_dollar_and_letters_exist);
    return UNITY_END();
}
