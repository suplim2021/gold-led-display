#include "IsoTime.h"

namespace gold {

int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static bool readDigits(const char*& p, int count, int& out) {
    out = 0;
    for (int i = 0; i < count; ++i) {
        if (*p < '0' || *p > '9') return false;
        out = out * 10 + (*p - '0');
        ++p;
    }
    return true;
}

static bool expect(const char*& p, char c) {
    if (*p != c) return false;
    ++p;
    return true;
}

bool parseIso8601(const char* s, int64_t& epochOut) {
    if (!s) return false;
    const char* p = s;
    int year, month, day, hour, minute, second = 0;
    if (!readDigits(p, 4, year) || !expect(p, '-') || !readDigits(p, 2, month) ||
        !expect(p, '-') || !readDigits(p, 2, day)) {
        return false;
    }
    if (*p != 'T' && *p != ' ') return false;
    ++p;
    if (!readDigits(p, 2, hour) || !expect(p, ':') || !readDigits(p, 2, minute)) return false;
    if (*p == ':') {
        ++p;
        if (!readDigits(p, 2, second)) return false;
        if (*p == '.') {  // fractional seconds: ignore
            ++p;
            if (*p < '0' || *p > '9') return false;
            while (*p >= '0' && *p <= '9') ++p;
        }
    }
    int offsetSec = 0;
    if (*p == 'Z' || *p == 'z') {
        ++p;
    } else if (*p == '+' || *p == '-') {
        const int sign = (*p == '-') ? -1 : 1;
        ++p;
        int oh, om;
        if (!readDigits(p, 2, oh)) return false;
        if (*p == ':') ++p;
        if (!readDigits(p, 2, om)) return false;
        if (oh > 14 || om > 59) return false;
        offsetSec = sign * (oh * 3600 + om * 60);
    } else {
        return false;  // timezone is mandatory, we refuse to guess
    }
    if (*p != '\0') return false;

    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 ||
        second > 60) {
        return false;
    }
    static const unsigned char kDaysInMonth[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (day > kDaysInMonth[month - 1]) return false;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && day == 29 && !leap) return false;

    const int64_t days = daysFromCivil(year, (unsigned)month, (unsigned)day);
    epochOut = days * 86400 + hour * 3600 + minute * 60 + second - offsetSec;
    return true;
}

}  // namespace gold
