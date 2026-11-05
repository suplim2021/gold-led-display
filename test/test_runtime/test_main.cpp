// Runtime state tests: Wi-Fi loss, reconnect, API outage, restart with last
// known price, NTP failure, stale timestamp, mode A/B screen selection.
// (Logic only - the real Wi-Fi / flash are exercised on hardware.)
#include <unity.h>

#include "Brightness.h"
#include "PriceTracker.h"
#include "ScreenSelector.h"

using namespace gold;

void setUp() {}
void tearDown() {}

static const int64_t kAnnounced = 1793852760LL;  // 2026-11-05T04:26:00Z
static const int64_t kNow = kAnnounced + 15 * 60;  // 15 min later

static GoldPrice samplePrice() {
    GoldPrice p;
    p.buyBar = 65250;
    p.sellBar = 65450;
    p.change = 1050;
    p.hasChange = true;
    p.updatedAtEpoch = kAnnounced;
    p.buyOrnament = 63945;
    p.sellOrnament = 66250;
    p.hasOrnament = true;
    p.trend = 1;
    p.hasTrend = true;
    p.worldSpot = 4139;
    p.worldTrend = -1;
    p.hasWorld = true;
    return p;
}

static Screen screenFor(const PriceTracker& t, uint32_t ms, int64_t epoch,
                        DisplayMode mode = DisplayMode::SellOnly) {
    ScreenInput in;
    in.hasPrice = t.hasPrice();
    in.price = t.price();
    in.fresh = t.isFresh(ms, epoch);
    in.fault = t.currentFault(ms, epoch);
    in.mode = mode;
    in.nowMs = ms;
    return selectScreen(in);
}

// ---------------------------------------------------------------- boot ----

void test_boot_shows_dashes_without_data() {
    PriceTracker t;
    Screen s = screenFor(t, 1000, 0);
    TEST_ASSERT_EQUAL(ScreenKind::Status, s.kind);  // Wi-Fi not up yet -> E1
    TEST_ASSERT_EQUAL(Fault::WiFi, s.fault);
    t.setWifiConnected(true);
    s = screenFor(t, 1000, 0);
    TEST_ASSERT_EQUAL(ScreenKind::Booting, s.kind);
}

void test_fresh_fetch_shows_sell_price_clean() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    TEST_ASSERT_TRUE(t.isFresh(6000, kNow));
    Screen s = screenFor(t, 6000, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Price, s.kind);
    TEST_ASSERT_EQUAL_INT32(65450, s.value);
    TEST_ASSERT_FALSE(s.stale);
    TEST_ASSERT_EQUAL(PriceMarker::None, s.marker);
}

// --------------------------------------------------------------- Wi-Fi ----

void test_wifi_disconnected_marks_stale_and_reports_e1() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    t.setWifiConnected(false);
    TEST_ASSERT_FALSE(t.isFresh(6000, kNow));
    TEST_ASSERT_EQUAL(Fault::WiFi, t.currentFault(6000, kNow));
    // shows the last price with the stale underline most of the time ...
    Screen s = screenFor(t, 10000 + 1000, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Price, s.kind);
    TEST_ASSERT_TRUE(s.stale);
    // ... and flashes E1 in the last 2 s of each 10 s cycle
    s = screenFor(t, 10000 + 8500, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Status, s.kind);
    TEST_ASSERT_EQUAL(Fault::WiFi, s.fault);
}

void test_network_reconnected_becomes_fresh_after_fetch() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    t.setWifiConnected(false);
    TEST_ASSERT_FALSE(t.isFresh(600000, kNow));
    t.setWifiConnected(true);
    TEST_ASSERT_FALSE(t.isFresh(600000, kNow));  // still old data
    t.onFetchSuccess(samplePrice(), 601000);
    TEST_ASSERT_TRUE(t.isFresh(602000, kNow));
    TEST_ASSERT_EQUAL(Fault::None, t.currentFault(602000, kNow));
}

// ---------------------------------------------------------------- API -----

void test_single_api_failure_is_tolerated() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    t.onFetchFailure(Fault::Api, 65000);
    TEST_ASSERT_TRUE(t.isFresh(70000, kNow));
    TEST_ASSERT_EQUAL(Fault::None, t.currentFault(70000, kNow));
}

void test_api_unavailable_becomes_stale_e2() {
    FreshnessPolicy pol;
    pol.maxFetchAgeMs = 180000;
    PriceTracker t(pol);
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    for (uint32_t ms = 65000; ms < 300000; ms += 30000) t.onFetchFailure(Fault::Api, ms);
    TEST_ASSERT_FALSE(t.isFresh(190000, kNow));
    TEST_ASSERT_EQUAL(Fault::Api, t.currentFault(190000, kNow));
    TEST_ASSERT_TRUE(t.hasPrice());  // last price kept
    TEST_ASSERT_EQUAL_INT32(65450, t.price().sellBar);
}

void test_api_unavailable_without_any_price_shows_status_only() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchFailure(Fault::Data, 5000);
    Screen s = screenFor(t, 6000, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Status, s.kind);
    TEST_ASSERT_EQUAL(Fault::Data, s.fault);
    Frame f;
    TEST_ASSERT_TRUE(renderScreen(f, s));
}

void test_retry_backoff_is_bounded() {
    TEST_ASSERT_EQUAL_UINT32(15000, backoffDelayMs(0, 15000, 300000));
    TEST_ASSERT_EQUAL_UINT32(30000, backoffDelayMs(1, 15000, 300000));
    TEST_ASSERT_EQUAL_UINT32(60000, backoffDelayMs(2, 15000, 300000));
    TEST_ASSERT_EQUAL_UINT32(240000, backoffDelayMs(4, 15000, 300000));
    TEST_ASSERT_EQUAL_UINT32(300000, backoffDelayMs(5, 15000, 300000));
    TEST_ASSERT_EQUAL_UINT32(300000, backoffDelayMs(255, 15000, 300000));
}

// ------------------------------------------------- restart / persistence ----

void test_restart_restores_last_price_as_stale() {
    PriceTracker t;
    t.restore(samplePrice());
    t.setWifiConnected(true);
    TEST_ASSERT_TRUE(t.hasPrice());
    TEST_ASSERT_FALSE(t.isFresh(1000, kNow));
    TEST_ASSERT_FALSE(t.needsPersist());  // came from flash: no write back
    Screen s = screenFor(t, 1000, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Price, s.kind);
    TEST_ASSERT_TRUE(s.stale);
    TEST_ASSERT_EQUAL_INT32(65450, s.value);
}

void test_restored_price_and_api_down_flashes_error() {
    PriceTracker t;
    t.restore(samplePrice());
    t.setWifiConnected(true);
    t.onFetchFailure(Fault::Api, 2000);
    TEST_ASSERT_EQUAL(Fault::Api, t.currentFault(3000, kNow));
    Screen s = screenFor(t, 9000, kNow);
    TEST_ASSERT_EQUAL(ScreenKind::Status, s.kind);
}

void test_persist_only_when_price_changes() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.restore(samplePrice());
    t.onFetchSuccess(samplePrice(), 1000);
    TEST_ASSERT_FALSE(t.needsPersist());  // same as flash copy
    GoldPrice p2 = samplePrice();
    p2.sellBar = 65550;
    p2.buyBar = 65350;
    p2.updatedAtEpoch += 600;
    t.onFetchSuccess(p2, 61000);
    TEST_ASSERT_TRUE(t.needsPersist());
    t.markPersisted();
    t.onFetchSuccess(p2, 121000);
    TEST_ASSERT_FALSE(t.needsPersist());
}

// ------------------------------------------------------------ clock -------

void test_ntp_sync_failure_skips_timestamp_checks() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    TEST_ASSERT_TRUE(t.isFresh(6000, 0));  // epoch 0 = clock unknown
    TEST_ASSERT_EQUAL(Fault::None, t.currentFault(6000, 0));
    // but fetch-age staleness still works without a clock
    TEST_ASSERT_FALSE(t.isFresh(5000 + 181000, 0));
}

void test_stale_timestamp_reports_e4() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 5000);
    const int64_t fiveDaysLater = kAnnounced + 5LL * 86400;
    TEST_ASSERT_FALSE(t.isFresh(6000, fiveDaysLater));
    TEST_ASSERT_EQUAL(Fault::OldData, t.currentFault(6000, fiveDaysLater));
    // weekend (2 days) is still fine
    TEST_ASSERT_TRUE(t.isFresh(6000, kAnnounced + 2LL * 86400));
}

void test_future_timestamp_rejected() {
    PriceTracker t;
    t.setWifiConnected(true);
    GoldPrice p = samplePrice();
    p.updatedAtEpoch = kNow + 3600;
    t.onFetchSuccess(p, 5000);
    TEST_ASSERT_FALSE(t.isFresh(6000, kNow));
    TEST_ASSERT_EQUAL(Fault::OldData, t.currentFault(6000, kNow));
}

void test_millis_rollover() {
    PriceTracker t;
    t.setWifiConnected(true);
    const uint32_t nearWrap = 0xFFFFFFFFu - 1000u;
    t.onFetchSuccess(samplePrice(), nearWrap);
    TEST_ASSERT_TRUE(t.isFresh(nearWrap + 60000u, kNow));  // wraps around
    TEST_ASSERT_FALSE(t.isFresh(nearWrap + 200000u, kNow));
}

// ------------------------------------------------------------- modes ------

void test_mode_b_alternates_sell_and_buy() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 0);
    Screen a = screenFor(t, 1000, kNow, DisplayMode::SellBuy);
    Screen b = screenFor(t, 6000, kNow, DisplayMode::SellBuy);
    Screen c = screenFor(t, 11000, kNow, DisplayMode::SellBuy);
    TEST_ASSERT_EQUAL(PriceMarker::Sell, a.marker);
    TEST_ASSERT_EQUAL_INT32(65450, a.value);
    TEST_ASSERT_EQUAL(PriceMarker::Buy, b.marker);
    TEST_ASSERT_EQUAL_INT32(65250, b.value);
    TEST_ASSERT_EQUAL(PriceMarker::Sell, c.marker);
}

void test_render_screen_never_fails_for_known_states() {
    Frame f;
    Screen s;
    TEST_ASSERT_TRUE(renderScreen(f, s));  // booting
    s.kind = ScreenKind::Status;
    for (uint8_t i = 1; i <= 4; ++i) {
        s.fault = (Fault)i;
        TEST_ASSERT_TRUE(renderScreen(f, s));
    }
    s.kind = ScreenKind::Price;
    s.value = 1234567;  // too wide -> fallback "------"
    TEST_ASSERT_FALSE(renderScreen(f, s));
    TEST_ASSERT_TRUE(f.litCount() > 0);
}

void test_mode2_thai_then_world() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 0);
    // default timing: Thai 8 s, world 4 s
    Screen a = screenFor(t, 1000, kNow, DisplayMode::ThaiWorld);
    Screen b = screenFor(t, 7900, kNow, DisplayMode::ThaiWorld);
    Screen c = screenFor(t, 9000, kNow, DisplayMode::ThaiWorld);
    Screen d = screenFor(t, 12500, kNow, DisplayMode::ThaiWorld);
    TEST_ASSERT_EQUAL(ScreenKind::Price, a.kind);
    TEST_ASSERT_EQUAL_INT32(65450, a.value);
    TEST_ASSERT_EQUAL_INT8(1, a.trend);
    TEST_ASSERT_EQUAL(ScreenKind::Price, b.kind);
    TEST_ASSERT_EQUAL(ScreenKind::World, c.kind);
    TEST_ASSERT_EQUAL_INT32(4139, c.value);
    TEST_ASSERT_EQUAL_INT8(-1, c.trend);
    TEST_ASSERT_EQUAL(ScreenKind::Price, d.kind);
    Frame f;
    TEST_ASSERT_TRUE(renderScreen(f, c));
}

void test_mode2_without_world_shows_thai_only() {
    PriceTracker t;
    t.setWifiConnected(true);
    GoldPrice p = samplePrice();
    p.hasWorld = false;
    t.onFetchSuccess(p, 0);
    for (uint32_t ms = 0; ms < 24000; ms += 500)
        TEST_ASSERT_EQUAL(ScreenKind::Price, screenFor(t, ms, kNow, DisplayMode::ThaiWorld).kind);
}

void test_ornament_option() {
    ScreenInput in;
    in.hasPrice = true;
    in.price = samplePrice();
    in.fresh = true;
    in.mode = DisplayMode::SellOnly;
    in.useOrnament = true;
    TEST_ASSERT_EQUAL_INT32(66250, selectScreen(in).value);
    in.price.hasOrnament = false;  // not available -> falls back to bar
    TEST_ASSERT_EQUAL_INT32(65450, selectScreen(in).value);
}

void test_trend_can_be_disabled() {
    ScreenInput in;
    in.hasPrice = true;
    in.price = samplePrice();
    in.fresh = true;
    in.mode = DisplayMode::SellOnly;
    in.showTrend = false;
    TEST_ASSERT_EQUAL_INT8(0, selectScreen(in).trend);
}

void test_trend_fallback_when_api_has_no_history() {
    PriceTracker t;
    t.setWifiConnected(true);
    GoldPrice p = samplePrice();
    p.hasTrend = false;
    p.trend = 0;
    t.onFetchSuccess(p, 0);
    TEST_ASSERT_EQUAL_INT8(0, t.price().trend);  // nothing to compare yet
    p.sellBar += 100;
    p.buyBar += 100;
    t.onFetchSuccess(p, 60000);
    TEST_ASSERT_EQUAL_INT8(1, t.price().trend);
    t.onFetchSuccess(p, 120000);  // same price: keep last direction
    TEST_ASSERT_EQUAL_INT8(1, t.price().trend);
    p.sellBar -= 50;
    p.buyBar -= 50;
    t.onFetchSuccess(p, 180000);
    TEST_ASSERT_EQUAL_INT8(-1, t.price().trend);
}

void test_world_change_alone_does_not_write_flash() {
    PriceTracker t;
    t.setWifiConnected(true);
    t.onFetchSuccess(samplePrice(), 0);
    t.markPersisted();
    GoldPrice p = samplePrice();
    p.worldSpot += 7;
    t.onFetchSuccess(p, 60000);
    TEST_ASSERT_FALSE(t.needsPersist());
    TEST_ASSERT_EQUAL_INT32(4146, t.price().worldSpot);  // but it is displayed
}

void test_setup_portal_screen() {
    ScreenInput in;
    in.setupPortal = true;
    in.hasPrice = true;
    in.price = samplePrice();
    Screen s = selectScreen(in);
    TEST_ASSERT_EQUAL(ScreenKind::Setup, s.kind);
    Frame f;
    TEST_ASSERT_TRUE(renderScreen(f, s));
}

void test_night_dimming() {
    BrightnessPolicy b;  // day 1, night 0, 22:00-06:00, UTC+7
    // 2026-11-05 04:26 UTC = 11:26 Thai -> day
    TEST_ASSERT_EQUAL(11, localHour(kAnnounced, b.utcOffsetSec));
    TEST_ASSERT_EQUAL_UINT8(1, intensityFor(kAnnounced, b));
    // 15:30 UTC = 22:30 Thai -> night
    const int64_t day0 = kAnnounced - (4 * 3600 + 26 * 60);  // 00:00 UTC
    TEST_ASSERT_EQUAL_UINT8(0, intensityFor(day0 + 15 * 3600 + 30 * 60, b));
    // 22:59 UTC = 05:59 Thai next day -> night ; 23:00 UTC = 06:00 -> day
    TEST_ASSERT_EQUAL_UINT8(0, intensityFor(day0 + 22 * 3600 + 59 * 60, b));
    TEST_ASSERT_EQUAL_UINT8(1, intensityFor(day0 + 23 * 3600, b));
    // 14:59 UTC = 21:59 Thai -> day ; 15:00 UTC = 22:00 -> night
    TEST_ASSERT_EQUAL_UINT8(1, intensityFor(day0 + 14 * 3600 + 59 * 60, b));
    TEST_ASSERT_EQUAL_UINT8(0, intensityFor(day0 + 15 * 3600, b));
    // clock unknown -> day brightness
    TEST_ASSERT_EQUAL_UINT8(1, intensityFor(0, b));
    // non-wrapping window and disabled window
    TEST_ASSERT_TRUE(isNightHour(2, 1, 5));
    TEST_ASSERT_FALSE(isNightHour(5, 1, 5));
    TEST_ASSERT_FALSE(isNightHour(3, 4, 4));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_shows_dashes_without_data);
    RUN_TEST(test_fresh_fetch_shows_sell_price_clean);
    RUN_TEST(test_wifi_disconnected_marks_stale_and_reports_e1);
    RUN_TEST(test_network_reconnected_becomes_fresh_after_fetch);
    RUN_TEST(test_single_api_failure_is_tolerated);
    RUN_TEST(test_api_unavailable_becomes_stale_e2);
    RUN_TEST(test_api_unavailable_without_any_price_shows_status_only);
    RUN_TEST(test_retry_backoff_is_bounded);
    RUN_TEST(test_restart_restores_last_price_as_stale);
    RUN_TEST(test_restored_price_and_api_down_flashes_error);
    RUN_TEST(test_persist_only_when_price_changes);
    RUN_TEST(test_ntp_sync_failure_skips_timestamp_checks);
    RUN_TEST(test_stale_timestamp_reports_e4);
    RUN_TEST(test_future_timestamp_rejected);
    RUN_TEST(test_millis_rollover);
    RUN_TEST(test_mode_b_alternates_sell_and_buy);
    RUN_TEST(test_render_screen_never_fails_for_known_states);
    RUN_TEST(test_mode2_thai_then_world);
    RUN_TEST(test_mode2_without_world_shows_thai_only);
    RUN_TEST(test_ornament_option);
    RUN_TEST(test_trend_can_be_disabled);
    RUN_TEST(test_trend_fallback_when_api_has_no_history);
    RUN_TEST(test_world_change_alone_does_not_write_flash);
    RUN_TEST(test_setup_portal_screen);
    RUN_TEST(test_night_dimming);
    return UNITY_END();
}
