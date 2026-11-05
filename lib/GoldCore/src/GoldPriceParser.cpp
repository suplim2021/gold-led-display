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

static int8_t signOf(int32_t v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

ParseError parseThaiGoldToday(const char* body, size_t length, GoldPrice& out) {
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
    filter["history"][0]["sellBar"] = true;  // [0] = applies to every element
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

    int32_t bo = 0, so = 0;
    if (readRounded(current["buyOrnament"], bo) && readRounded(current["sellOrnament"], so) &&
        plausible(bo) && plausible(so) && bo <= so) {
        p.buyOrnament = bo;
        p.sellOrnament = so;
        p.hasOrnament = true;
    }

    // Direction of the latest announcement round. NOTE: current.change is the
    // change vs. the previous DAY; the per-round change is in history[].change.
    // Only trusted when the last history entry is the same price as `current`.
    JsonArrayConst history = doc["history"].as<JsonArrayConst>();
    if (!history.isNull() && history.size() > 0) {
        JsonVariantConst last = history[history.size() - 1];
        int32_t hSell = 0, hChange = 0;
        bool present = false;
        if (readPrice(last["sellBar"], hSell, present) && hSell == p.sellBar &&
            readRounded(last["change"], hChange)) {
            p.trend = signOf(hChange);
            p.hasTrend = true;
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
