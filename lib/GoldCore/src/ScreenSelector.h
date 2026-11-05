// Decides WHAT to show (independent of how it is drawn).
//
//  Mode A (SellOnly)   : sell price, clean, no indicators.
//  Mode B (Alternate)  : sell (marker top-left) / buy (marker bottom-left),
//                        switching every alternatePeriodMs.
//  Mode C (stale/error): price with dotted underline; every statusEveryMs the
//                        status code (E1..E4) is shown for statusShowMs.
//                        With no price at all, the status code is shown
//                        permanently. Before the first result: "----".
#pragma once

#include <stdint.h>

#include "DisplayRenderer.h"
#include "Frame.h"
#include "GoldPrice.h"

namespace gold {

enum class DisplayMode : uint8_t { SellOnly = 0, Alternate = 1 };

enum class ScreenKind : uint8_t { Booting, Price, Status };

struct Screen {
    ScreenKind kind = ScreenKind::Booting;
    int32_t value = 0;
    PriceMarker marker = PriceMarker::None;
    bool stale = false;
    Fault fault = Fault::None;

    bool operator==(const Screen& o) const {
        return kind == o.kind && value == o.value && marker == o.marker && stale == o.stale &&
               fault == o.fault;
    }
};

struct ScreenTiming {
    uint32_t alternatePeriodMs = 5000;
    uint32_t statusEveryMs = 10000;
    uint32_t statusShowMs = 2000;
};

struct ScreenInput {
    bool hasPrice = false;
    GoldPrice price;
    bool fresh = false;
    Fault fault = Fault::None;
    DisplayMode mode = DisplayMode::SellOnly;
    uint32_t nowMs = 0;
};

Screen selectScreen(const ScreenInput& in, const ScreenTiming& timing = ScreenTiming());

// Draws a Screen into the frame. Returns false if it could not be drawn
// (e.g. price > 6 digits); in that case the frame shows "------".
bool renderScreen(Frame& frame, const Screen& screen);

}  // namespace gold
