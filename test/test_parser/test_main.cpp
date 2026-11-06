// API parser tests (run on PC: pio test -e native)
#include <string.h>
#include <unity.h>

#include <string>

#include "../fixtures/live_response.h"
#include "../fixtures/live_response_2.h"
#include "GoldPriceParser.h"
#include "GoldPriceProvider.h"
#include "IsoTime.h"

using namespace gold;

void setUp() {}
void tearDown() {}

static ParseError parse(const char* json, GoldPrice& out) {
    return parseThaiGoldToday(json, json ? strlen(json) : 0, out);
}

// ----------------------------------------------------------- valid JSON ----

void test_live_response_captured_from_api() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok, parse(LIVE_RESPONSE, p));
    TEST_ASSERT_EQUAL_INT32(65250, p.buyBar);
    TEST_ASSERT_EQUAL_INT32(65450, p.sellBar);
    TEST_ASSERT_TRUE(p.hasChange);
    TEST_ASSERT_EQUAL_INT32(1050, p.change);
    // 2026-11-05T04:26:00.000Z
    TEST_ASSERT_EQUAL_INT64(1793852760LL, p.updatedAtEpoch);
    // ornament
    TEST_ASSERT_TRUE(p.hasOrnament);
    TEST_ASSERT_EQUAL_INT32(63945, p.buyOrnament);
    TEST_ASSERT_EQUAL_INT32(66250, p.sellOrnament);
    // trend comes from history[last].change (-50), NOT current.change (+1050 = vs. yesterday)
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(-1, p.trend);
    // world spot 4138.55 -> 4139, change +91.1
    TEST_ASSERT_TRUE(p.hasWorld);
    TEST_ASSERT_EQUAL_INT32(4139, p.worldSpot);
    TEST_ASSERT_EQUAL_INT8(1, p.worldTrend);
    TEST_ASSERT_TRUE(p.worldUpdatedEpoch > p.updatedAtEpoch);
}

static const char* kBase =
    "\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}";

static ParseError parseWith(const char* extra, GoldPrice& p) {
    std::string json = std::string("{") + kBase + extra + "}";
    return parseThaiGoldToday(json.c_str(), json.size(), p);
}

void test_trend_from_history_up_down_flat() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseWith(",\"history\":[{\"sellBar\":65300,\"change\":-100},"
                                "{\"sellBar\":65450,\"change\":150}]", p));
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(1, p.trend);
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseWith(",\"history\":[{\"sellBar\":65450,\"change\":-50}]", p));
    TEST_ASSERT_EQUAL_INT8(-1, p.trend);
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseWith(",\"history\":[{\"sellBar\":65450,\"change\":0}]", p));
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(0, p.trend);
}

void test_trend_ignored_when_history_does_not_match_current() {
    GoldPrice p;
    // last history round is a different price -> do not trust it
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseWith(",\"history\":[{\"sellBar\":65500,\"change\":50}]", p));
    TEST_ASSERT_FALSE(p.hasTrend);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith(",\"history\":[]", p));
    TEST_ASSERT_FALSE(p.hasTrend);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith(",\"history\":\"oops\"", p));
    TEST_ASSERT_FALSE(p.hasTrend);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith("", p));
    TEST_ASSERT_FALSE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(0, p.trend);
}

void test_world_optional_and_validated() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith("", p));
    TEST_ASSERT_FALSE(p.hasWorld);
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseWith(",\"world\":{\"spotOz\":4131.49,\"change\":-12.5}", p));
    TEST_ASSERT_TRUE(p.hasWorld);
    TEST_ASSERT_EQUAL_INT32(4131, p.worldSpot);
    TEST_ASSERT_EQUAL_INT8(-1, p.worldTrend);
    // broken world data never rejects the Thai price
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith(",\"world\":{\"spotOz\":\"n/a\"}", p));
    TEST_ASSERT_FALSE(p.hasWorld);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith(",\"world\":{\"spotOz\":5}", p));
    TEST_ASSERT_FALSE(p.hasWorld);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith(",\"world\":null", p));
    TEST_ASSERT_FALSE(p.hasWorld);
    TEST_ASSERT_EQUAL_INT32(65450, p.sellBar);
}

void test_ornament_optional_and_rounded() {
    GoldPrice p;
    std::string json =
        "{\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"buyOrnament\":63944.88,"
        "\"sellOrnament\":66250,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}";
    TEST_ASSERT_EQUAL(ParseError::Ok, parseThaiGoldToday(json.c_str(), json.size(), p));
    TEST_ASSERT_TRUE(p.hasOrnament);
    TEST_ASSERT_EQUAL_INT32(63945, p.buyOrnament);
    TEST_ASSERT_EQUAL(ParseError::Ok, parseWith("", p));
    TEST_ASSERT_FALSE(p.hasOrnament);
}

void test_documented_sample_response() {
    const char* json =
        "{\"current\":{\"buyBar\":70350,\"sellBar\":70550,\"buyOrnament\":68943,"
        "\"sellOrnament\":71350,\"change\":50,\"changePercent\":0.07,\"updateRound\":2,"
        "\"updatedAt\":\"2026-05-17T09:30:00.000Z\"},\"world\":{\"spotOz\":3245.50}}";
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok, parse(json, p));
    TEST_ASSERT_EQUAL_INT32(70350, p.buyBar);
    TEST_ASSERT_EQUAL_INT32(70550, p.sellBar);
    TEST_ASSERT_EQUAL_INT32(50, p.change);
}

void test_change_is_optional() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450,"
                            "\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
                            p));
    TEST_ASSERT_FALSE(p.hasChange);
}

void test_negative_change() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"change\":-100,"
                            "\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
                            p));
    TEST_ASSERT_EQUAL_INT32(-100, p.change);
}

void test_prices_with_comma_separators() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parse("{\"current\":{\"buyBar\":\"65,250\",\"sellBar\":\"65,450.00\","
                            "\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
                            p));
    TEST_ASSERT_EQUAL_INT32(65250, p.buyBar);
    TEST_ASSERT_EQUAL_INT32(65450, p.sellBar);

    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parse("{\"current\":{\"buyBar\":\"99,950\",\"sellBar\":\"100,150\","
                            "\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
                            p));
    TEST_ASSERT_EQUAL_INT32(100150, p.sellBar);
}

void test_price_text_helper() {
    int32_t v = 0;
    TEST_ASSERT_TRUE(parsePriceText("65450", v));
    TEST_ASSERT_EQUAL_INT32(65450, v);
    TEST_ASSERT_TRUE(parsePriceText("1,000,000", v));
    TEST_ASSERT_EQUAL_INT32(1000000, v);
    TEST_ASSERT_TRUE(parsePriceText("65 450", v));
    TEST_ASSERT_FALSE(parsePriceText("", v));
    TEST_ASSERT_FALSE(parsePriceText(",", v));
    TEST_ASSERT_FALSE(parsePriceText("-65450", v));
    TEST_ASSERT_FALSE(parsePriceText("65450.50", v));
    TEST_ASSERT_FALSE(parsePriceText("65450.", v));
    TEST_ASSERT_FALSE(parsePriceText("abc", v));
    TEST_ASSERT_FALSE(parsePriceText("N/A", v));
    TEST_ASSERT_FALSE(parsePriceText("1234567890123", v));
}

// --------------------------------------------------------- invalid input ----

void test_invalid_json() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::InvalidJson, parse("{\"current\":{\"buyBar\":65250,", p));
    TEST_ASSERT_EQUAL(ParseError::InvalidJson, parse("<html>502 Bad Gateway</html>", p));
    TEST_ASSERT_EQUAL(ParseError::InvalidJson, parse("[1,2,3]", p));
}

void test_empty_response() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::EmptyBody, parse("", p));
    TEST_ASSERT_EQUAL(ParseError::EmptyBody, parse("  \r\n", p));
    TEST_ASSERT_EQUAL(ParseError::EmptyBody, parseThaiGoldToday(nullptr, 0, p));
}

void test_missing_current() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::MissingCurrent, parse("{}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingCurrent, parse("{\"current\":null}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingCurrent, parse("{\"current\":[1]}", p));
}

void test_missing_price() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parse("{\"current\":{\"buyBar\":65250,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parse("{\"current\":{\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}", p));
}

void test_schema_change_renamed_fields() {
    // e.g. the provider renames fields -> must not be accepted silently
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parse("{\"current\":{\"bar\":{\"buy\":65250,\"sell\":65450},"
                            "\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
                            p));
    TEST_ASSERT_EQUAL(ParseError::MissingCurrent,
                      parse("{\"data\":{\"buyBar\":65250,\"sellBar\":65450}}", p));
}

void test_invalid_price_values() {
    GoldPrice p;
    const char* bad[] = {
        "{\"current\":{\"buyBar\":null,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":\"N/A\",\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":0,\"sellBar\":0,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":-1,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":65250.5,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":65650,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":999000,\"sellBar\":1000050,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
        "{\"current\":{\"buyBar\":true,\"sellBar\":65450,\"updatedAt\":\"2026-11-05T04:26:00Z\"}}",
    };
    for (const char* json : bad) {
        const ParseError e = parse(json, p);
        // null counts as missing; everything else is an invalid value
        TEST_ASSERT_TRUE_MESSAGE(e == ParseError::InvalidPrice || e == ParseError::MissingPrice, json);
    }
}

void test_output_untouched_on_error() {
    GoldPrice p;
    p.sellBar = 12345;
    parse("{\"current\":{\"buyBar\":65250}}", p);
    TEST_ASSERT_EQUAL_INT32(12345, p.sellBar);
}

void test_missing_or_bad_timestamp() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::MissingTimestamp,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450}}", p));
    TEST_ASSERT_EQUAL(ParseError::InvalidTimestamp,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"updatedAt\":1793852760}}", p));
    TEST_ASSERT_EQUAL(ParseError::InvalidTimestamp,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"updatedAt\":\"yesterday\"}}", p));
    TEST_ASSERT_EQUAL(ParseError::InvalidTimestamp,
                      parse("{\"current\":{\"buyBar\":65250,\"sellBar\":65450,\"updatedAt\":\"2026-11-05 11:26\"}}", p));
}

// ------------------------------------------------------------ ISO 8601 ----

void test_iso8601_variants() {
    int64_t t = 0;
    TEST_ASSERT_TRUE(parseIso8601("1970-01-01T00:00:00Z", t));
    TEST_ASSERT_EQUAL_INT64(0, t);
    TEST_ASSERT_TRUE(parseIso8601("2026-11-05T04:26:00.000Z", t));
    TEST_ASSERT_EQUAL_INT64(1793852760LL, t);
    TEST_ASSERT_TRUE(parseIso8601("2026-11-05T11:26:00+07:00", t));
    TEST_ASSERT_EQUAL_INT64(1793852760LL, t);
    TEST_ASSERT_TRUE(parseIso8601("2026-11-05T11:26:00+0700", t));
    TEST_ASSERT_EQUAL_INT64(1793852760LL, t);
    TEST_ASSERT_TRUE(parseIso8601("2024-02-29T00:00:00Z", t));  // leap day
    TEST_ASSERT_EQUAL_INT64(1709164800LL, t);
    TEST_ASSERT_FALSE(parseIso8601("2026-02-29T00:00:00Z", t));
    TEST_ASSERT_FALSE(parseIso8601("2026-13-01T00:00:00Z", t));
    TEST_ASSERT_FALSE(parseIso8601("2026-11-05T04:26:00", t));  // no timezone
    TEST_ASSERT_FALSE(parseIso8601("2026-11-05T04:26:00Zjunk", t));
    TEST_ASSERT_FALSE(parseIso8601("", t));
    TEST_ASSERT_FALSE(parseIso8601(nullptr, t));
}

// ------------------------------------------------- HTTP error / timeout ----

void test_http_status_mapping() {
    TEST_ASSERT_EQUAL(Fault::None, faultForHttpStatus(200));
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(404));
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(429));
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(500));
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(503));
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(-1));   // connection refused
    TEST_ASSERT_EQUAL(Fault::Api, faultForHttpStatus(-11));  // HTTPC_ERROR_READ_TIMEOUT
}

// A fake provider shows how another data source can be plugged in.
class FakeProvider : public GoldPriceProvider {
public:
    int status = 200;
    const char* body = "";
    const char* name() const override { return "fake"; }
    FetchResult fetch(GoldPrice& out) override {
        FetchResult r;
        r.httpStatus = status;
        r.fault = faultForHttpStatus(status);
        if (!r.ok()) return r;
        const ParseError e = parseThaiGoldToday(body, strlen(body), out);
        if (e != ParseError::Ok) r.fault = e == ParseError::EmptyBody ? Fault::Api : Fault::Data;
        return r;
    }
};

void test_provider_abstraction() {
    FakeProvider fp;
    GoldPriceProvider& prov = fp;
    GoldPrice p;
    fp.body = LIVE_RESPONSE;
    TEST_ASSERT_TRUE(prov.fetch(p).ok());
    fp.status = -11;  // timeout
    TEST_ASSERT_EQUAL(Fault::Api, prov.fetch(p).fault);
    fp.status = 200;
    fp.body = "{\"oops\":1}";
    TEST_ASSERT_EQUAL(Fault::Data, prov.fetch(p).fault);
    fp.body = "";
    TEST_ASSERT_EQUAL(Fault::Api, prov.fetch(p).fault);
}

// ---------------------------------------------------- ornament primary ----

static ParseError parseOrn(const std::string& json, GoldPrice& p) {
    return parseThaiGoldToday(json.c_str(), json.size(), p, ThaiPrice::Ornament);
}

void test_live_response_2_ornament_primary() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseThaiGoldToday(LIVE_RESPONSE_2, strlen(LIVE_RESPONSE_2), p,
                                         ThaiPrice::Ornament));
    TEST_ASSERT_TRUE(p.hasOrnament);
    TEST_ASSERT_EQUAL_INT32(65300, p.sellOrnament);  // shown on the display
    TEST_ASSERT_EQUAL_INT32(63014, p.buyOrnament);
    TEST_ASSERT_EQUAL_INT32(64500, p.sellBar);
    TEST_ASSERT_EQUAL_INT32(800, p.sellOrnament - p.sellBar);
    // last history round: sellOrnament 65300, change +50 -> up
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(1, p.trend);
    TEST_ASSERT_EQUAL_INT32(-750, p.change);  // day change
    TEST_ASSERT_TRUE(p.hasWorld);
    TEST_ASSERT_EQUAL_INT32(4071, p.worldSpot);
}

void test_first_live_response_ornament_primary() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok, parseThaiGoldToday(LIVE_RESPONSE, strlen(LIVE_RESPONSE), p,
                                                         ThaiPrice::Ornament));
    TEST_ASSERT_EQUAL_INT32(66250, p.sellOrnament);
    TEST_ASSERT_EQUAL_INT8(-1, p.trend);  // history sellOrnament 66250, change -50
}

static const std::string kHead =
    "{\"current\":{\"buyBar\":64300,\"sellBar\":64500,\"updatedAt\":\"2026-11-06T02:31:25Z\"";

void test_ornament_primary_required() {
    GoldPrice p;
    // missing completely -> E3 (MissingPrice), never falls back to the bar price
    TEST_ASSERT_EQUAL(ParseError::MissingPrice, parseOrn(kHead + "}}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parseOrn(kHead + ",\"buyOrnament\":63014}}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parseOrn(kHead + ",\"sellOrnament\":65300}}", p));
    TEST_ASSERT_EQUAL(ParseError::MissingPrice,
                      parseOrn(kHead + ",\"sellOrnament\":null,\"buyOrnament\":63014}}", p));
    // the bar-primary parser still accepts the same body
    std::string json = kHead + "}}";
    TEST_ASSERT_EQUAL(ParseError::Ok, parseThaiGoldToday(json.c_str(), json.size(), p));
}

void test_ornament_primary_validated() {
    GoldPrice p;
    const char* bad[] = {
        ",\"sellOrnament\":\"N/A\",\"buyOrnament\":63014}}",
        ",\"sellOrnament\":65300.5,\"buyOrnament\":63014}}",   // fractional sell price
        ",\"sellOrnament\":0,\"buyOrnament\":0}}",
        ",\"sellOrnament\":63000,\"buyOrnament\":65300}}",     // buy > sell
        ",\"sellOrnament\":1065300,\"buyOrnament\":63014}}",   // 7 digits
        ",\"sellOrnament\":true,\"buyOrnament\":63014}}",
    };
    for (const char* tail : bad) {
        TEST_ASSERT_EQUAL_MESSAGE(ParseError::InvalidPrice, parseOrn(kHead + tail, p), tail);
    }
}

void test_ornament_primary_accepts_valid_variants() {
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":\"65,300\",\"buyOrnament\":63020.12}}", p));
    TEST_ASSERT_EQUAL_INT32(65300, p.sellOrnament);
    TEST_ASSERT_EQUAL_INT32(63020, p.buyOrnament);  // rounded
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":65300.0,\"buyOrnament\":63014}}", p));
    TEST_ASSERT_EQUAL_INT32(65300, p.sellOrnament);
}

void test_ornament_trend_matches_ornament_history() {
    GoldPrice p;
    // history matched on sellOrnament when ornament is primary
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":65300,\"buyOrnament\":63014},"
                                       "\"history\":[{\"sellBar\":64500,\"sellOrnament\":65300,"
                                       "\"change\":-50}]}", p));
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(-1, p.trend);
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":65300,\"buyOrnament\":63014},"
                                       "\"history\":[{\"sellBar\":64500,\"sellOrnament\":65250,"
                                       "\"change\":50}]}", p));
    TEST_ASSERT_FALSE(p.hasTrend);  // no entry with the displayed price -> not trusted
}

void test_trend_when_history_is_ahead_of_current() {
    // Real situation 2026-11-06 09:36: history already had round 4 (65250, -50)
    // while current still showed round 3 (65300, +50). The arrow must belong
    // to the price on the display.
    GoldPrice p;
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":65300,\"buyOrnament\":63014},"
                                       "\"history\":[{\"sellOrnament\":65250,\"change\":-850},"
                                       "{\"sellOrnament\":65300,\"change\":50},"
                                       "{\"sellOrnament\":65250,\"change\":-50}]}", p));
    TEST_ASSERT_TRUE(p.hasTrend);
    TEST_ASSERT_EQUAL_INT8(1, p.trend);
    // only the last 3 rounds are searched
    TEST_ASSERT_EQUAL(ParseError::Ok,
                      parseOrn(kHead + ",\"sellOrnament\":65300,\"buyOrnament\":63014},"
                                       "\"history\":[{\"sellOrnament\":65300,\"change\":50},"
                                       "{\"sellOrnament\":1,\"change\":1},{\"sellOrnament\":2,\"change\":1},"
                                       "{\"sellOrnament\":3,\"change\":1}]}", p));
    TEST_ASSERT_FALSE(p.hasTrend);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_live_response_captured_from_api);
    RUN_TEST(test_documented_sample_response);
    RUN_TEST(test_live_response_2_ornament_primary);
    RUN_TEST(test_first_live_response_ornament_primary);
    RUN_TEST(test_ornament_primary_required);
    RUN_TEST(test_ornament_primary_validated);
    RUN_TEST(test_ornament_primary_accepts_valid_variants);
    RUN_TEST(test_ornament_trend_matches_ornament_history);
    RUN_TEST(test_trend_when_history_is_ahead_of_current);
    RUN_TEST(test_trend_from_history_up_down_flat);
    RUN_TEST(test_trend_ignored_when_history_does_not_match_current);
    RUN_TEST(test_world_optional_and_validated);
    RUN_TEST(test_ornament_optional_and_rounded);
    RUN_TEST(test_change_is_optional);
    RUN_TEST(test_negative_change);
    RUN_TEST(test_prices_with_comma_separators);
    RUN_TEST(test_price_text_helper);
    RUN_TEST(test_invalid_json);
    RUN_TEST(test_empty_response);
    RUN_TEST(test_missing_current);
    RUN_TEST(test_missing_price);
    RUN_TEST(test_schema_change_renamed_fields);
    RUN_TEST(test_invalid_price_values);
    RUN_TEST(test_output_untouched_on_error);
    RUN_TEST(test_missing_or_bad_timestamp);
    RUN_TEST(test_iso8601_variants);
    RUN_TEST(test_http_status_mapping);
    RUN_TEST(test_provider_abstraction);
    return UNITY_END();
}
