#include "PriceTracker.h"

namespace gold {

void PriceTracker::restore(const GoldPrice& p) {
    price_ = p;
    hasPrice_ = true;
    fetchedThisSession_ = false;
    dirty_ = false;  // it came from flash, nothing to write back
}

void PriceTracker::onFetchSuccess(const GoldPrice& p, uint32_t nowMs) {
    if (!hasPrice_ || !price_.sameAs(p)) dirty_ = true;
    price_ = p;
    hasPrice_ = true;
    fetchedThisSession_ = true;
    lastSuccessMs_ = nowMs;
    lastFault_ = Fault::None;
    failures_ = 0;
}

void PriceTracker::onFetchFailure(Fault f, uint32_t nowMs) {
    (void)nowMs;
    lastFault_ = f;
    if (failures_ < 255) ++failures_;
}

bool PriceTracker::timestampOk(int64_t epochNow) const {
    if (epochNow <= 0) return true;  // clock unknown: cannot judge
    const int64_t age = epochNow - price_.updatedAtEpoch;
    if (age > policy_.maxPriceAgeSec) return false;
    if (age < -policy_.maxFutureSkewSec) return false;
    return true;
}

bool PriceTracker::isFresh(uint32_t nowMs, int64_t epochNow) const {
    if (!hasPrice_ || !fetchedThisSession_ || !wifiConnected_) return false;
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
