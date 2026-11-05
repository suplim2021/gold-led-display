#include "Brightness.h"

namespace gold {

int localHour(int64_t epochUtc, int32_t utcOffsetSec) {
    if (epochUtc <= 0) return -1;
    int64_t local = epochUtc + utcOffsetSec;
    int64_t secOfDay = local % 86400;
    if (secOfDay < 0) secOfDay += 86400;
    return (int)(secOfDay / 3600);
}

bool isNightHour(int hour, uint8_t startHour, uint8_t endHour) {
    if (hour < 0 || startHour == endHour) return false;
    if (startHour < endHour) return hour >= startHour && hour < endHour;
    return hour >= startHour || hour < endHour;  // wraps over midnight
}

uint8_t intensityFor(int64_t epochUtc, const BrightnessPolicy& p) {
    const int h = localHour(epochUtc, p.utcOffsetSec);
    const uint8_t v = isNightHour(h, p.nightStartHour, p.nightEndHour) ? p.nightIntensity
                                                                       : p.dayIntensity;
    return v > 15 ? 15 : v;
}

}  // namespace gold
