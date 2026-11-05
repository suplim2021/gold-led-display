// Parser for https://www.thaigoldtoday.com/api/gold-price
//
// Verified schema (API docs + live response, 2026-11-05):
//   {
//     "current": {
//       "buyBar": 65250, "sellBar": 65450,          <- gold bar 96.5%, THB
//       "buyOrnament": ..., "sellOrnament": ...,
//       "updatedAt": "2026-11-05T04:26:00.000Z",    <- ISO-8601 UTC
//       "change": 1050, "changePercent": 1.63,
//       "updateRound": 2                            <- in docs, absent live
//     },
//     "history": [ {..., "sellBar": 65400, "change": -50}, ... ]  <- per round
//     "world": { "spotOz": 4131.67, "change": 84.2, "updatedAt": "..." }
//   }
// Only `current.buyBar`, `current.sellBar` and `current.updatedAt` are
// mandatory. Everything else is optional and never rejects the bar price:
//   * current.change       = change vs. previous DAY (verified on live data)
//   * history[last].change = change of the latest round -> trend arrow
//   * ornament prices (may contain decimals -> rounded)
//   * world.spotOz (USD/oz, rounded), sign of world.change
// Numbers given as strings with thousands separators ("65,450") are accepted.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "GoldPrice.h"

namespace gold {

enum class ParseError : uint8_t {
    Ok = 0,
    EmptyBody,
    InvalidJson,
    MissingCurrent,
    MissingPrice,
    InvalidPrice,      // not a number, out of range, fractional, or buy > sell
    MissingTimestamp,
    InvalidTimestamp,
};

const char* parseErrorName(ParseError e);

constexpr int32_t kMinPlausiblePrice = 10000;   // sanity bounds, THB
constexpr int32_t kMaxPlausiblePrice = 999999;  // also the 6-digit display limit
constexpr int32_t kMinPlausibleWorld = 200;     // USD/oz sanity bounds
constexpr int32_t kMaxPlausibleWorld = 99999;   // "$" + 5 digits still fits

ParseError parseThaiGoldToday(const char* body, size_t length, GoldPrice& out);

// Parses "65450", "65,450", "65450.00", " 65 450 " into an integer.
// Rejects empty strings, signs, fractions other than .0/.00 and other junk.
bool parsePriceText(const char* text, int32_t& out);

}  // namespace gold
