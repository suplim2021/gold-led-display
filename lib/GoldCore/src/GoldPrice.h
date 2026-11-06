#pragma once

#include <stdint.h>

namespace gold {

// Thai gold 96.5%, baht per 1 baht-weight, plus world spot price.
struct GoldPrice {
    int32_t buyBar = 0;          // ราคารับซื้อทองแท่ง
    int32_t sellBar = 0;         // ราคาขายออกทองแท่ง
    int32_t buyOrnament = 0;     // ราคารับซื้อทองรูปพรรณ (optional)
    int32_t sellOrnament = 0;    // ราคาขายออกทองรูปพรรณ (optional)
    bool hasOrnament = false;
    int32_t change = 0;          // current.change: change vs. previous day (THB)
    bool hasChange = false;
    int8_t trend = 0;            // direction of the LAST announcement round: -1 / 0 / +1
    bool hasTrend = false;       // trend came from the API history
    int64_t updatedAtEpoch = 0;  // current.updatedAt, Unix seconds (UTC); see sameThaiPrice()

    // Gold Traders Association announcement that produced this price (Thai time).
    int16_t announcedMinute = -1;  // minutes since midnight, -1 = unknown
    uint8_t announcedDay = 0;      // 1..31, 0 = unknown
    uint8_t announcedMonth = 0;    // 1..12, 0 = unknown

    bool hasAnnouncedTime() const { return announcedMinute >= 0; }
    bool hasAnnouncedDate() const { return announcedDay > 0 && announcedMonth > 0; }

    int32_t worldSpot = 0;       // world spot, USD per troy ounce (rounded)
    int8_t worldTrend = 0;       // sign of world.change (vs. previous close)
    bool hasWorld = false;
    int64_t worldUpdatedEpoch = 0;

    // True when everything worth persisting is unchanged. updatedAtEpoch is
    // deliberately ignored: the API sometimes returns the server time there,
    // which changes on every request and would otherwise cause a flash write
    // every poll (verified 2026-11-06).
    bool sameThaiPrice(const GoldPrice& o) const {
        return buyBar == o.buyBar && sellBar == o.sellBar && buyOrnament == o.buyOrnament &&
               sellOrnament == o.sellOrnament && hasOrnament == o.hasOrnament &&
               change == o.change && hasChange == o.hasChange && trend == o.trend &&
               announcedMinute == o.announcedMinute;
    }
};

// Why the last attempt to obtain a price failed. Values are shown on the
// display as status codes "E1".."E4", so keep the numbers stable.
enum class Fault : uint8_t {
    None = 0,
    WiFi = 1,     // E1: no Wi-Fi connection
    Api = 2,      // E2: HTTP error, timeout, TLS/DNS failure
    Data = 3,     // E3: response not parseable / schema changed / bad values
    OldData = 4,  // E4: announcement timestamp is unreasonably old or in the future
};

}  // namespace gold
