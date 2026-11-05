// Night dimming: pick the LED intensity from the local time of day.
#pragma once

#include <stdint.h>

namespace gold {

struct BrightnessPolicy {
    uint8_t dayIntensity = 1;     // 0..15
    uint8_t nightIntensity = 0;   // 0 = dimmest the MAX7219 can do (LEDs stay on)
    uint8_t nightStartHour = 22;  // local time, inclusive
    uint8_t nightEndHour = 6;     // local time, exclusive
    int32_t utcOffsetSec = 7 * 3600;  // Thailand (no DST)
};

// Local hour 0..23 for a UTC epoch, or -1 if the clock is not known (epoch <= 0).
int localHour(int64_t epochUtc, int32_t utcOffsetSec);

// True if the local hour falls in [start, end), wrapping over midnight.
bool isNightHour(int hour, uint8_t startHour, uint8_t endHour);

// Day intensity when the clock is unknown.
uint8_t intensityFor(int64_t epochUtc, const BrightnessPolicy& p);

}  // namespace gold
