// 32x8 monochrome frame buffer. Pure C++ (no Arduino), so it can be unit
// tested on a PC and used by the pixel preview tool.
#pragma once

#include <stdint.h>

namespace gold {

struct Frame {
    static constexpr int kWidth = 32;
    static constexpr int kHeight = 8;

    // rows[y] bit x  ->  pixel at column x (0 = left), row y (0 = top).
    uint32_t rows[kHeight];
    // Number of attempted writes outside the 32x8 area (overflow detection).
    uint16_t clipped;

    Frame() { clear(); }

    void clear() {
        for (int y = 0; y < kHeight; ++y) rows[y] = 0;
        clipped = 0;
    }

    static bool inBounds(int x, int y) {
        return x >= 0 && x < kWidth && y >= 0 && y < kHeight;
    }

    // Returns false (and counts a clip) when the pixel is outside the frame.
    bool set(int x, int y, bool on = true) {
        if (!inBounds(x, y)) {
            ++clipped;
            return false;
        }
        if (on) {
            rows[y] |= (1UL << x);
        } else {
            rows[y] &= ~(1UL << x);
        }
        return true;
    }

    bool get(int x, int y) const {
        if (!inBounds(x, y)) return false;
        return (rows[y] >> x) & 1UL;
    }

    int litCount() const {
        int n = 0;
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) n += get(x, y) ? 1 : 0;
        }
        return n;
    }

    bool operator==(const Frame& o) const {
        for (int y = 0; y < kHeight; ++y) {
            if (rows[y] != o.rows[y]) return false;
        }
        return true;
    }
    bool operator!=(const Frame& o) const { return !(*this == o); }
};

}  // namespace gold
