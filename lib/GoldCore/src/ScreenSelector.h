// Decides WHAT to show (independent of how it is drawn).
//
//  Mode 0 (SellOnly)  : Thai sell price only.
//  Mode 1 (SellBuy)   : Thai sell (marker top-left) / buy (marker bottom-left),
//                       switching every alternatePeriodMs.
//  Mode 2 (ThaiWorld) : Thai sell for thaiShowMs, world spot "$4132" for
//                       worldShowMs, then the announcement date "6/11" and time
//                       "10:46" (pages with unknown data are skipped).
//  Trend arrow        : triangle top-right = last round went UP,
//                       bottom-right = DOWN (Thai: latest announcement round;
//                       world: vs. previous close).
//  Stale / error      : price with dotted underline; every statusEveryMs the
//                       status code (E1..E4) is shown for statusShowMs.
//                       With no price at all the status code stays.
//                       Before the first result: "----". Wi-Fi setup: "AP".
#pragma once

#include <stdint.h>

#include "DisplayRenderer.h"
#include "Frame.h"
#include "GoldPrice.h"

namespace gold {

enum class DisplayMode : uint8_t { SellOnly = 0, SellBuy = 1, ThaiWorld = 2 };
constexpr uint8_t kDisplayModeCount = 3;

enum class ScreenKind : uint8_t { Booting, Price, World, Status, Setup, Date, Time };

struct Screen {
    ScreenKind kind = ScreenKind::Booting;
    int32_t value = 0;  // price; Date: day * 100 + month; Time: minutes since midnight
    PriceMarker marker = PriceMarker::None;
    bool stale = false;
    int8_t trend = 0;
    Fault fault = Fault::None;

    bool operator==(const Screen& o) const {
        return kind == o.kind && value == o.value && marker == o.marker && stale == o.stale &&
               trend == o.trend && fault == o.fault;
    }
};

struct ScreenTiming {
    uint32_t alternatePeriodMs = 5000;  // mode 1
    uint32_t thaiShowMs = 10000;        // mode 2
    uint32_t worldShowMs = 4000;        // mode 2
    uint32_t dateShowMs = 2000;         // mode 2
    uint32_t timeShowMs = 2000;         // mode 2
    uint32_t statusEveryMs = 10000;
    uint32_t statusShowMs = 2000;
};

struct ScreenInput {
    bool hasPrice = false;
    GoldPrice price;
    bool fresh = false;
    Fault fault = Fault::None;
    DisplayMode mode = DisplayMode::ThaiWorld;
    uint32_t nowMs = 0;
    bool useOrnament = false;  // show ทองรูปพรรณ instead of ทองแท่ง (if available)
    bool showTrend = true;
    bool setupPortal = false;  // Wi-Fi setup access point is active
};

Screen selectScreen(const ScreenInput& in, const ScreenTiming& timing = ScreenTiming());

// Draws a Screen into the frame. Returns false if it could not be drawn
// (e.g. price > 6 digits); in that case the frame shows "------".
bool renderScreen(Frame& frame, const Screen& screen);

}  // namespace gold
