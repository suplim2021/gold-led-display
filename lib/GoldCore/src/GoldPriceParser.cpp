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

    // Keep only the fields we need; the full response (~4-5 KB, with history)
    // is never materialised in RAM as a JSON tree.
    JsonDocument filter;
    filter["current"]["buyBar"] = true;
    filter["current"]["sellBar"] = true;
    filter["current"]["updatedAt"] = true;
    filter["current"]["change"] = true;

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

    JsonVariantConst ch = current["change"];
    if (ch.is<long long>()) {
        p.change = (int32_t)ch.as<long long>();
        p.hasChange = true;
    } else if (ch.is<double>()) {
        p.change = (int32_t)lround(ch.as<double>());
        p.hasChange = true;
    }

    out = p;
    return ParseError::Ok;
}

}  // namespace gold
