#include "ScreenSelector.h"

namespace gold {

Screen selectScreen(const ScreenInput& in, const ScreenTiming& t) {
    Screen s;
    if (in.setupPortal) {
        s.kind = ScreenKind::Setup;
        return s;
    }
    if (!in.hasPrice) {
        if (in.fault == Fault::None) {
            s.kind = ScreenKind::Booting;
        } else {
            s.kind = ScreenKind::Status;
            s.fault = in.fault;
        }
        return s;
    }

    const bool ornament = in.useOrnament && in.price.hasOrnament;
    const int32_t sell = ornament ? in.price.sellOrnament : in.price.sellBar;
    const int32_t buy = ornament ? in.price.buyOrnament : in.price.buyBar;

    s.kind = ScreenKind::Price;
    s.stale = !in.fresh;
    s.value = sell;
    s.trend = in.showTrend ? in.price.trend : 0;

    if (in.mode == DisplayMode::SellBuy && t.alternatePeriodMs > 0) {
        const bool showBuy = ((in.nowMs / t.alternatePeriodMs) % 2) == 1;
        s.marker = showBuy ? PriceMarker::Buy : PriceMarker::Sell;
        s.value = showBuy ? buy : sell;
    } else if (in.mode == DisplayMode::ThaiWorld && in.price.hasWorld && t.worldShowMs > 0) {
        const uint32_t cycle = t.thaiShowMs + t.worldShowMs;
        if ((in.nowMs % cycle) >= t.thaiShowMs) {
            s.kind = ScreenKind::World;
            s.value = in.price.worldSpot;
            s.trend = in.showTrend ? in.price.worldTrend : 0;
        }
    }

    if (s.stale && in.fault != Fault::None && t.statusEveryMs > 0 &&
        (in.nowMs % t.statusEveryMs) >= (t.statusEveryMs - t.statusShowMs)) {
        Screen st;
        st.kind = ScreenKind::Status;
        st.fault = in.fault;
        return st;
    }
    return s;
}

bool renderScreen(Frame& frame, const Screen& screen) {
    switch (screen.kind) {
        case ScreenKind::Booting:
            return renderCenteredText(frame, "----").ok;
        case ScreenKind::Setup:
            return renderCenteredText(frame, "AP").ok;
        case ScreenKind::Status: {
            char text[3] = {'E', (char)('0' + (uint8_t)screen.fault % 10), '\0'};
            return renderCenteredText(frame, text).ok;
        }
        case ScreenKind::Price:
            if (renderPrice(frame, screen.value, screen.marker, screen.stale, screen.trend).ok) {
                return true;
            }
            break;
        case ScreenKind::World:
            if (renderWorldPrice(frame, screen.value, screen.stale, screen.trend).ok) return true;
            break;
    }
    renderCenteredText(frame, "------");
    return false;
}

}  // namespace gold
