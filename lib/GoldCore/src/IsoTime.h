#pragma once

#include <stdint.h>

namespace gold {

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's algorithm).
int64_t daysFromCivil(int64_t y, unsigned m, unsigned d);

// Parses ISO-8601 timestamps such as
//   2026-11-05T04:26:00.000Z, 2026-11-05T11:26:00+07:00, 2026-11-05T04:26:00Z
// into Unix seconds (UTC). Returns false on malformed input.
bool parseIso8601(const char* s, int64_t& epochOut);

}  // namespace gold
