// Abstraction over the price source, so the data source can be replaced
// without touching the display / state logic.
#pragma once

#include "GoldPrice.h"

namespace gold {

struct FetchResult {
    Fault fault = Fault::None;  // None on success
    int httpStatus = 0;         // HTTP status, or negative transport error
    const char* detail = "";    // short human readable reason (static string)
    bool ok() const { return fault == Fault::None; }
};

class GoldPriceProvider {
public:
    virtual ~GoldPriceProvider() = default;
    virtual const char* name() const = 0;
    // Blocking fetch. Fills `out` only on success.
    virtual FetchResult fetch(GoldPrice& out) = 0;
};

// Maps an HTTP status (or negative transport error code) to a Fault.
// 200 -> None; anything else -> Api.
inline Fault faultForHttpStatus(int status) { return status == 200 ? Fault::None : Fault::Api; }

}  // namespace gold
