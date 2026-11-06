#include "PriceTracker.h"

namespace gold {

void PriceTracker::restore(const GoldPrice& p) {
    price_ = p;
    hasPrice_ = true;
    fetchedThisSession_ = false;
    dirty_ = false;  // it came from flash, nothing to write back
}

void PriceTracker::onFetchSuccess(const GoldPrice& fetched, uint32_t nowMs) {
    GoldPrice p = fetched;
    if (!p.hasTrend && hasPrice_) {
        // API history unavailable: derive the direction ourselves.
        if (p.sellBar != price_.sellBar) {
            p.trend = p.sellBar > price_.sellBar ? 1 : -1;
        } else {
            p.trend = price_.trend;
        }
    }
    // The API does not always reveal the date (see the parser). Within one
    // announcement round keep the date already known, so the page does not flicker.
    if (hasPrice_ && !p.hasAnnouncedDate() && p.hasAnnouncedTime() &&
        p.announcedMinute == price_.announcedMinute) {
        p.announcedDay = price_.announcedDay;
        p.announcedMonth = price_.announcedMonth;
    }
    if (!hasPrice_ || !price_.sameThaiPrice(p)) dirty_ = true;  // world price is not persisted
    price_ = p;
    hasPrice_ = true;
    fetchedThisSession_ = true;
    fetchWindowOpen_ = true;
    lastSuccessMs_ = nowMs;
    lastFault_ = Fault::None;
    failures_ = 0;
}

void PriceTracker::onFetchFailure(Fault f, uint32_t nowMs) {
    (void)nowMs;
    lastFault_ = f;
    if (failures_ < 255) ++failures_;
}

void PriceTracker::tick(uint32_t nowMs) {
    if (fetchWindowOpen_ && (uint32_t)(nowMs - lastSuccessMs_) > policy_.maxFetchAgeMs) {
        fetchWindowOpen_ = false;
    }
}

bool PriceTracker::timestampOk(int64_t epochNow) const {
    if (epochNow <= 0) return true;  // clock unknown: cannot judge
    const int64_t age = epochNow - price_.updatedAtEpoch;
    if (age > policy_.maxPriceAgeSec) return false;
    if (age < -policy_.maxFutureSkewSec) return false;
    return true;
}

bool PriceTracker::isFresh(uint32_t nowMs, int64_t epochNow) const {
    if (!hasPrice_ || !fetchedThisSession_ || !fetchWindowOpen_ || !wifiConnected_) return false;
    if ((uint32_t)(nowMs - lastSuccessMs_) > policy_.maxFetchAgeMs) return false;
    return timestampOk(epochNow);
}

Fault PriceTracker::currentFault(uint32_t nowMs, int64_t epochNow) const {
    if (!wifiConnected_) return Fault::WiFi;
    if (!hasPrice_) return lastFault_;  // None while still booting
    if (!timestampOk(epochNow)) return Fault::OldData;
    // A single failed poll shortly after a good one is not reported while the
    // data is still within maxFetchAgeMs.
    if (isFresh(nowMs, epochNow)) return Fault::None;
    if (lastFault_ != Fault::None) return lastFault_;
    // Fetched earlier but nothing (not even a failure) recorded since: treat
    // as API trouble. A price restored from flash with no attempt yet: None.
    return fetchedThisSession_ ? Fault::Api : Fault::None;
}

uint32_t backoffDelayMs(uint8_t attempt, uint32_t baseMs, uint32_t maxMs) {
    uint64_t d = baseMs;
    for (uint8_t i = 0; i < attempt && d < maxMs; ++i) d <<= 1;
    return d > maxMs ? maxMs : (uint32_t)d;
}

}  // namespace gold
