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
    } else if (in.mode == DisplayMode::ThaiWorld) {
        const GoldPrice& p = in.price;
        const uint32_t worldMs = p.hasWorld ? t.worldShowMs : 0;
        const uint32_t timeMs = p.hasAnnouncedTime() ? t.timeShowMs : 0;
        const uint32_t dateMs = p.hasAnnouncedTime() && p.hasAnnouncedDate() ? t.dateShowMs : 0;
        const uint32_t cycle = t.thaiShowMs + worldMs + dateMs + timeMs;
        const uint32_t phase = cycle > 0 ? in.nowMs % cycle : 0;
        if (phase >= t.thaiShowMs + worldMs + dateMs) {
            s.kind = ScreenKind::Time;
            s.value = p.announcedMinute;
            s.trend = 0;
        } else if (phase >= t.thaiShowMs + worldMs) {
            s.kind = ScreenKind::Date;
            s.value = p.announcedDay * 100 + p.announcedMonth;
            s.trend = 0;
        } else if (phase >= t.thaiShowMs) {
            s.kind = ScreenKind::World;
            s.value = p.worldSpot;
            s.trend = in.showTrend ? p.worldTrend : 0;
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
        case ScreenKind::Date:
            if (renderDate(frame, screen.value / 100, screen.value % 100, screen.stale).ok) {
                return true;
            }
            break;
        case ScreenKind::Time:
            if (renderTimeOfDay(frame, screen.value, screen.stale).ok) return true;
            break;
    }
    renderCenteredText(frame, "------");
    return false;
}

}  // namespace gold
