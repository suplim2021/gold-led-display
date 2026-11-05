#include "ScreenSelector.h"

namespace gold {

Screen selectScreen(const ScreenInput& in, const ScreenTiming& t) {
    Screen s;
    if (!in.hasPrice) {
        if (in.fault == Fault::None) {
            s.kind = ScreenKind::Booting;
        } else {
            s.kind = ScreenKind::Status;
            s.fault = in.fault;
        }
        return s;
    }

    s.kind = ScreenKind::Price;
    s.stale = !in.fresh;
    s.value = in.price.sellBar;

    if (in.mode == DisplayMode::Alternate && t.alternatePeriodMs > 0) {
        const bool showBuy = ((in.nowMs / t.alternatePeriodMs) % 2) == 1;
        s.marker = showBuy ? PriceMarker::Buy : PriceMarker::Sell;
        s.value = showBuy ? in.price.buyBar : in.price.sellBar;
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
        case ScreenKind::Status: {
            char text[3] = {'E', (char)('0' + (uint8_t)screen.fault % 10), '\0'};
            return renderCenteredText(frame, text).ok;
        }
        case ScreenKind::Price: {
            if (renderPrice(frame, screen.value, screen.marker, screen.stale).ok) return true;
            renderCenteredText(frame, "------");
            return false;
        }
    }
    return false;
}

}  // namespace gold
