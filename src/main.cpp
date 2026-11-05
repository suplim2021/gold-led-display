// Thai Gold Price LED Display — ESP32 + MAX7219 4-in-1 (32x8)
//
// Boot: display -> config -> Wi-Fi -> NTP -> fetch -> parse -> show -> save -> poll.
// Data source: https://www.thaigoldtoday.com (ข้อมูล: thaigoldtoday.com)

#include <Arduino.h>
#include <time.h>

#include "AppConfig.h"
#include "GoldPriceClient.h"
#include "LedMatrix.h"
#include "PriceStore.h"
#include "PriceTracker.h"
#include "ScreenSelector.h"
#include "WiFiManager.h"

using namespace gold;

static constexpr int kBootButtonPin = 0;
static constexpr int64_t kClockValidAfter = 1700000000;  // 2023-11; earlier = not synced

static LedMatrix display(DISPLAY_HARDWARE_TYPE, DISPLAY_CS_PIN, DISPLAY_MODULES,
                         DISPLAY_MIRROR_X != 0, DISPLAY_FLIP_Y != 0);
static WiFiManager wifi;
static ThaiGoldTodayClient apiClient(GOLD_API_URL, GOLD_API_ALLOW_INSECURE_TLS != 0,
                                     HTTP_TIMEOUT_MS);
static GoldPriceProvider& provider = apiClient;
static PriceStore store;

static FreshnessPolicy makePolicy() {
    FreshnessPolicy p;
    p.maxFetchAgeMs = STALE_AFTER_MS;
    p.maxPriceAgeSec = MAX_PRICE_AGE_SEC;
    return p;
}
static PriceTracker tracker(makePolicy());

static DisplayMode mode = (DisplayMode)DISPLAY_MODE_DEFAULT;
static ScreenTiming timing;
static uint32_t nextFetchMs = 0;
static uint32_t lastLogMs = 0;

static int64_t epochNow() {
    const time_t t = time(nullptr);
    return (int64_t)t >= kClockValidAfter ? (int64_t)t : 0;
}

static void logPrice(const char* tag, const GoldPrice& p) {
    const time_t ts = (time_t)(p.updatedAtEpoch + 7 * 3600);  // show in Thai time
    struct tm tmv;
    gmtime_r(&ts, &tmv);
    Serial.printf("[%s] buy=%ld sell=%ld change=%ld updated=%04d-%02d-%02d %02d:%02d (ICT)\n", tag,
                  (long)p.buyBar, (long)p.sellBar, (long)p.change, tmv.tm_year + 1900,
                  tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
}

static void doFetch(uint32_t nowMs) {
    GoldPrice p;
    const FetchResult r = provider.fetch(p);
    nowMs = millis();  // fetch may take seconds
    if (r.ok()) {
        tracker.onFetchSuccess(p, nowMs);
        logPrice("api", p);
        if (tracker.needsPersist()) {
            if (store.save(tracker.price())) {
                tracker.markPersisted();
                Serial.println("[nvs] saved last price");
            }
        }
        nextFetchMs = nowMs + POLL_INTERVAL_MS;
    } else {
        tracker.onFetchFailure(r.fault, nowMs);
        const uint32_t wait =
            backoffDelayMs(tracker.consecutiveFailures() - 1, RETRY_BASE_MS, RETRY_MAX_MS);
        Serial.printf("[api] failed (E%u, %s, http=%d) failures=%u retry in %lus\n",
                      (unsigned)r.fault, r.detail, r.httpStatus, tracker.consecutiveFailures(),
                      (unsigned long)(wait / 1000));
        nextFetchMs = nowMs + wait;
    }
}

static void handleButton(uint32_t nowMs) {
#if ENABLE_BOOT_BUTTON_TOGGLE
    static bool lastPressed = false;
    static uint32_t lastChangeMs = 0;
    const bool pressed = digitalRead(kBootButtonPin) == LOW;
    if (pressed != lastPressed && nowMs - lastChangeMs > 50) {
        lastChangeMs = nowMs;
        lastPressed = pressed;
        if (pressed) {
            mode = (mode == DisplayMode::SellOnly) ? DisplayMode::Alternate : DisplayMode::SellOnly;
            store.saveMode(mode);
            Serial.printf("[ui] mode -> %s\n", mode == DisplayMode::SellOnly ? "A (sell)" : "B (sell/buy)");
        }
    }
#else
    (void)nowMs;
#endif
}

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println("\n[boot] Thai gold price LED display");

    // 1-2. Display
    if (!display.begin(DISPLAY_INTENSITY)) Serial.println("[display] init failed");
    Frame frame;
    renderScreen(frame, Screen());  // "----"
    display.show(frame, true);

    // 3. Configuration / persisted state
    pinMode(kBootButtonPin, INPUT_PULLUP);
    timing.alternatePeriodMs = ALTERNATE_PERIOD_MS;
    DisplayMode savedMode;
    if (store.loadMode(savedMode)) mode = savedMode;
    GoldPrice saved;
    if (store.load(saved)) {
        tracker.restore(saved);  // shown as STALE until a fresh fetch succeeds
        logPrice("nvs", saved);
    }

    // 4. Wi-Fi (non-blocking)
    wifi.begin(WIFI_SSID, WIFI_PASSWORD, DEVICE_HOSTNAME, WIFI_RETRY_BASE_MS, WIFI_RETRY_MAX_MS);

    // 5. NTP (UTC). SNTP runs in the background once the network is up.
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
}

void loop() {
    const uint32_t now = millis();

    wifi.loop(now);
    tracker.setWifiConnected(wifi.connected());
    if (wifi.takeJustConnected()) nextFetchMs = now;  // fetch right after (re)connect

    // 6-9. Fetch, validate, show, save
    if (wifi.connected() && (int32_t)(now - nextFetchMs) >= 0) doFetch(now);

    handleButton(now);

    const uint32_t t = millis();
    const int64_t epoch = epochNow();
    ScreenInput in;
    in.hasPrice = tracker.hasPrice();
    in.price = tracker.price();
    in.fresh = tracker.isFresh(t, epoch);
    in.fault = tracker.currentFault(t, epoch);
    in.mode = mode;
    in.nowMs = t;
    Frame frame;
    renderScreen(frame, selectScreen(in, timing));
    display.show(frame);

    if (t - lastLogMs > 30000) {
        lastLogMs = t;
        Serial.printf("[state] wifi=%d fresh=%d fault=E%u clock=%s heap=%u\n", wifi.connected(),
                      in.fresh, (unsigned)in.fault, epoch ? "synced" : "unsynced",
                      (unsigned)ESP.getFreeHeap());
    }
    delay(20);
}
