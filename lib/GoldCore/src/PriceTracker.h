// Holds the last known price and decides whether it may be shown as current.
//
// A price is "fresh" only when ALL of these hold:
//   * it was fetched successfully during this power-on session
//     (a price restored from flash after reboot is always stale until confirmed)
//   * the last successful fetch is no older than maxFetchAgeMs
//   * Wi-Fi is connected
//   * if the clock is synced: updatedAt is not older than maxPriceAgeSec
//     and not more than maxFutureSkewSec in the future
// Note: Thai gold prices do not change on weekends/holidays, so maxPriceAgeSec
// is intentionally generous (days), while maxFetchAgeMs (minutes) catches a
// dead connection.
#pragma once

#include <stdint.h>

#include "GoldPrice.h"

namespace gold {

struct FreshnessPolicy {
    uint32_t maxFetchAgeMs = 3UL * 60UL * 1000UL;   // 3 poll cycles
    int64_t maxPriceAgeSec = 4LL * 24 * 3600;       // long holiday weekend
    int64_t maxFutureSkewSec = 15LL * 60;
};

class PriceTracker {
public:
    explicit PriceTracker(const FreshnessPolicy& policy = FreshnessPolicy()) : policy_(policy) {}

    // Price loaded from flash at boot (never considered fresh by itself).
    void restore(const GoldPrice& p);

    void onFetchSuccess(const GoldPrice& p, uint32_t nowMs);
    void onFetchFailure(Fault f, uint32_t nowMs);
    void setWifiConnected(bool connected) { wifiConnected_ = connected; }

    bool hasPrice() const { return hasPrice_; }
    const GoldPrice& price() const { return price_; }

    // epochNow <= 0 means "clock not synced": timestamp checks are skipped.
    bool isFresh(uint32_t nowMs, int64_t epochNow) const;

    // Most relevant fault to show (None when fresh / nothing wrong yet).
    Fault currentFault(uint32_t nowMs, int64_t epochNow) const;

    // True when the stored copy differs from the current price
    // (used to avoid needless flash writes).
    bool needsPersist() const { return hasPrice_ && dirty_; }
    void markPersisted() { dirty_ = false; }

    uint8_t consecutiveFailures() const { return failures_; }

private:
    bool timestampOk(int64_t epochNow) const;

    FreshnessPolicy policy_;
    GoldPrice price_;
    bool hasPrice_ = false;
    bool fetchedThisSession_ = false;
    bool dirty_ = false;
    bool wifiConnected_ = false;
    uint32_t lastSuccessMs_ = 0;
    Fault lastFault_ = Fault::None;
    uint8_t failures_ = 0;
};

// Exponential backoff: base * 2^attempt, capped at maxMs. attempt 0 -> base.
uint32_t backoffDelayMs(uint8_t attempt, uint32_t baseMs, uint32_t maxMs);

}  // namespace gold
