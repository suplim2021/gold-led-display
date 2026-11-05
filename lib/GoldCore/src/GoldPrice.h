#pragma once

#include <stdint.h>

namespace gold {

// Thai gold bar 96.5%, baht per 1 baht-weight.
struct GoldPrice {
    int32_t buyBar = 0;          // ราคารับซื้อทองแท่ง
    int32_t sellBar = 0;         // ราคาขายออกทองแท่ง
    int32_t change = 0;          // change from previous round (if provided)
    bool hasChange = false;
    int64_t updatedAtEpoch = 0;  // announcement time, Unix seconds (UTC)

    bool sameAs(const GoldPrice& o) const {
        return buyBar == o.buyBar && sellBar == o.sellBar && change == o.change &&
               hasChange == o.hasChange && updatedAtEpoch == o.updatedAtEpoch;
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
