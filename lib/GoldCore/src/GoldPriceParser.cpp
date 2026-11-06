#include "GoldPriceParser.h"

#include <ArduinoJson.h>
#include <math.h>

#include "IsoTime.h"

namespace gold {

const char* parseErrorName(ParseError e) {
    switch (e) {
        case ParseError::Ok: return "ok";
        case ParseError::EmptyBody: return "empty body";
        case ParseError::InvalidJson: return "invalid json";
        case ParseError::MissingCurrent: return "missing 'current'";
        case ParseError::MissingPrice: return "missing price";
        case ParseError::InvalidPrice: return "invalid price";
        case ParseError::MissingTimestamp: return "missing updatedAt";
        case ParseError::InvalidTimestamp: return "invalid updatedAt";
    }
    return "?";
}

bool parsePriceText(const char* text, int32_t& out) {
    if (!text) return false;
    int64_t value = 0;
    int digits = 0;
    const char* p = text;
    for (; *p; ++p) {
        const char c = *p;
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            if (++digits > 9) return false;
        } else if (c == ',' || c == ' ') {
            continue;
        } else if (c == '.') {
            break;
        } else {
            return false;
        }
    }
    if (*p == '.') {  // only an all-zero fraction is allowed
        ++p;
        if (!*p) return false;
        for (; *p; ++p) {
            if (*p != '0') return false;
        }
    }
    if (digits == 0) return false;
    out = (int32_t)value;
    return true;
}

static bool readPrice(JsonVariantConst v, int32_t& out, bool& present) {
    present = !v.isNull();
    if (!present) return false;
    if (v.is<const char*>()) return parsePriceText(v.as<const char*>(), out);
    if (v.is<long long>()) {
        const long long n = v.as<long long>();
        if (n < 0 || n > 2000000000LL) return false;
        out = (int32_t)n;
        return true;
    }
    if (v.is<double>()) {
        const double d = v.as<double>();
        if (!(d >= 0.0) || d > 2.0e9 || d != floor(d)) return false;
        out = (int32_t)d;
        return true;
    }
    return false;
}

static bool plausible(int32_t p) { return p >= kMinPlausiblePrice && p <= kMaxPlausiblePrice; }

// Lenient number reader for optional fields: rounds fractions (e.g. 63899.4).
static bool readRounded(JsonVariantConst v, int32_t& out) {
    if (v.is<long long>()) {
        const long long n = v.as<long long>();
        if (n < -2000000000LL || n > 2000000000LL) return false;
        out = (int32_t)n;
        return true;
    }
    if (v.is<double>()) {
        const double d = v.as<double>();
        if (!(d > -2.0e9 && d < 2.0e9)) return false;
        out = (int32_t)lround(d);
        return true;
    }
    if (v.is<const char*>()) return parsePriceText(v.as<const char*>(), out);
    return false;
}

static constexpr size_t kTrendLookback = 3;

static constexpr int64_t kThaiUtcOffsetSec = 7 * 3600;  // history[].time is Thai time

static int8_t signOf(int32_t v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

// "HH:MM" -> minutes since midnight, -1 if malformed.
static int clockMinutes(const char* text) {
    if (!text) return -1;
    const char* p = text;
    int hour = 0;
    int digits = 0;
    for (; *p >= '0' && *p <= '9' && digits < 3; ++p, ++digits) hour = hour * 10 + (*p - '0');
    if (digits < 1 || digits > 2 || hour > 23 || *p != ':') return -1;
    ++p;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9' || p[2] != '\0') return -1;
    const int minute = (p[0] - '0') * 10 + (p[1] - '0');
    return minute > 59 ? -1 : hour * 60 + minute;
}

struct Round {
    int32_t buyBar = 0, sellBar = 0, buyOrnament = 0, sellOrnament = 0;
    int minute = -1;
};

// One history entry, usable only when its time and all four prices are valid.
static bool readRound(JsonVariantConst e, Round& r) {
    r.minute = clockMinutes(e["time"].as<const char*>());
    return r.minute >= 0 && readRounded(e["buyBar"], r.buyBar) &&
           readRounded(e["sellBar"], r.sellBar) && readRounded(e["buyOrnament"], r.buyOrnament) &&
           readRounded(e["sellOrnament"], r.sellOrnament) && plausible(r.buyBar) &&
           plausible(r.sellBar) && plausible(r.buyOrnament) && plausible(r.sellOrnament) &&
           r.buyBar <= r.sellBar && r.buyOrnament <= r.sellOrnament;
}

ParseError parseThaiGoldToday(const char* body, size_t length, GoldPrice& out,
                              ThaiPrice primary) {
    if (!body || length == 0) return ParseError::EmptyBody;
    bool onlySpace = true;
    for (size_t i = 0; i < length; ++i) {
        const char c = body[i];
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            onlySpace = false;
            break;
        }
    }
    if (onlySpace) return ParseError::EmptyBody;

    // Keep only the fields we need; the rest of the response (weekly data,
    // ornament history, etc.) is never materialised in RAM.
    JsonDocument filter;
    filter["current"]["buyBar"] = true;
    filter["current"]["sellBar"] = true;
    filter["current"]["buyOrnament"] = true;
    filter["current"]["sellOrnament"] = true;
    filter["current"]["updatedAt"] = true;
    filter["current"]["change"] = true;
    filter["history"][0]["time"] = true;  // [0] = applies to every element
    filter["history"][0]["buyBar"] = true;
    filter["history"][0]["sellBar"] = true;
    filter["history"][0]["buyOrnament"] = true;
    filter["history"][0]["sellOrnament"] = true;
    filter["history"][0]["change"] = true;
    filter["world"]["spotOz"] = true;
    filter["world"]["change"] = true;
    filter["world"]["updatedAt"] = true;

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, body, length, DeserializationOption::Filter(filter));
    if (err) return ParseError::InvalidJson;
    if (!doc.is<JsonObjectConst>()) return ParseError::InvalidJson;

    JsonObjectConst current = doc["current"].as<JsonObjectConst>();
    if (current.isNull()) return ParseError::MissingCurrent;

    GoldPrice p;
    bool buyPresent = false, sellPresent = false;
    const bool buyOk = readPrice(current["buyBar"], p.buyBar, buyPresent);
    const bool sellOk = readPrice(current["sellBar"], p.sellBar, sellPresent);
    if (!buyPresent || !sellPresent) return ParseError::MissingPrice;
    if (!buyOk || !sellOk) return ParseError::InvalidPrice;
    if (!plausible(p.buyBar) || !plausible(p.sellBar)) return ParseError::InvalidPrice;
    if (p.buyBar > p.sellBar) return ParseError::InvalidPrice;

    JsonVariantConst ts = current["updatedAt"];
    if (ts.isNull()) return ParseError::MissingTimestamp;
    if (!ts.is<const char*>()) return ParseError::InvalidTimestamp;
    if (!parseIso8601(ts.as<const char*>(), p.updatedAtEpoch)) return ParseError::InvalidTimestamp;

    // ---- optional fields: a problem here never rejects the bar price ----
    int32_t v = 0;
    if (readRounded(current["change"], v)) {
        p.change = v;
        p.hasChange = true;
    }

    // Ornament (ทองรูปพรรณ). Verified 2026-11-06: sellOrnament is an integer
    // (= sellBar + 800), buyOrnament is an integer in `current` but may carry
    // decimals in history (e.g. 62959.48), so the buy side is rounded.
    if (primary == ThaiPrice::Ornament) {
        // Primary price: required and strictly validated -> E3 if missing/odd,
        // never silently replaced by the bar price.
        bool soPresent = false;
        int32_t so = 0, bo = 0;
        const bool soOk = readPrice(current["sellOrnament"], so, soPresent);
        if (!soPresent || current["buyOrnament"].isNull()) return ParseError::MissingPrice;
        if (!soOk || !readRounded(current["buyOrnament"], bo)) return ParseError::InvalidPrice;
        if (!plausible(so) || !plausible(bo) || bo > so) return ParseError::InvalidPrice;
        p.buyOrnament = bo;
        p.sellOrnament = so;
        p.hasOrnament = true;
    } else {
        int32_t bo = 0, so = 0;
        if (readRounded(current["buyOrnament"], bo) && readRounded(current["sellOrnament"], so) &&
            plausible(bo) && plausible(so) && bo <= so) {
            p.buyOrnament = bo;
            p.sellOrnament = so;
            p.hasOrnament = true;
        }
    }

    // Latest announcement round. Verified 2026-11-06: `current` can lag several
    // rounds behind `history` (price 65150 from 10:10 while 65000 was announced
    // at 10:36), so the newest round wins, but only when `current` appears in
    // history. Otherwise history may belong to an earlier day, so `current` is
    // kept and no announcement time is claimed.
    // NOTE: current.change is the change vs. the previous DAY; the per-round
    // change is in history[].change.
    JsonArrayConst history = doc["history"].as<JsonArrayConst>();
    if (!history.isNull() && history.size() > 0) {
        const bool ornament = primary == ThaiPrice::Ornament;
        const char* const sellKey = ornament ? "sellOrnament" : "sellBar";
        const int32_t mine = ornament ? p.sellOrnament : p.sellBar;
        const size_t n = history.size();

        // updatedAt is sometimes a round's time and sometimes the request time.
        // Only the former can date the rounds, because history has no dates.
        const int64_t thaiSec = p.updatedAtEpoch + kThaiUtcOffsetSec;
        const int64_t thaiDay = thaiSec >= 0 ? thaiSec / 86400 : (thaiSec - 86399) / 86400;
        const int64_t secOfDay = thaiSec - thaiDay * 86400;
        bool currentInHistory = false;
        bool updatedAtIsRound = false;
        for (JsonVariantConst e : history) {
            int32_t sell = 0;
            if (readRounded(e[sellKey], sell) && sell == mine) currentInHistory = true;
            const int minute = clockMinutes(e["time"].as<const char*>());
            if (minute >= 0 && secOfDay % 60 == 0 && minute * 60 == secOfDay) {
                updatedAtIsRound = true;
            }
        }

        Round last;
        if (currentInHistory && readRound(history[n - 1], last)) {
            if ((ornament ? last.sellOrnament : last.sellBar) != mine) {
                p.buyBar = last.buyBar;
                p.sellBar = last.sellBar;
                p.buyOrnament = last.buyOrnament;
                p.sellOrnament = last.sellOrnament;
                p.hasOrnament = true;
            }
            p.announcedMinute = (int16_t)last.minute;
            if (updatedAtIsRound) {
                unsigned month = 0, day = 0;
                monthDayFromDays(thaiDay, month, day);
                p.announcedDay = (uint8_t)day;
                p.announcedMonth = (uint8_t)month;
            }
            int32_t change = 0;
            if (readRounded(history[n - 1]["change"], change)) {
                p.trend = signOf(change);
                p.hasTrend = true;
            }
        } else {
            // Direction of the round that produced the displayed price.
            const size_t lookback = n < kTrendLookback ? n : kTrendLookback;
            for (size_t k = 0; k < lookback; ++k) {
                JsonVariantConst e = history[n - 1 - k];
                int32_t hSell = 0, hChange = 0;
                bool present = false;
                if (readPrice(e[sellKey], hSell, present) && hSell == mine &&
                    readRounded(e["change"], hChange)) {
                    p.trend = signOf(hChange);
                    p.hasTrend = true;
                    break;
                }
            }
        }
    }

    JsonObjectConst world = doc["world"].as<JsonObjectConst>();
    if (!world.isNull()) {
        int32_t spot = 0;
        if (readRounded(world["spotOz"], spot) && spot >= kMinPlausibleWorld &&
            spot <= kMaxPlausibleWorld) {
            p.worldSpot = spot;
            p.hasWorld = true;
            int32_t wc = 0;
            if (readRounded(world["change"], wc)) p.worldTrend = signOf(wc);
            JsonVariantConst wts = world["updatedAt"];
            if (wts.is<const char*>()) parseIso8601(wts.as<const char*>(), p.worldUpdatedEpoch);
        }
    }

    out = p;
    return ParseError::Ok;
}

}  // namespace gold
