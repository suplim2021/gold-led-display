// Thai Gold Price LED Display — ESP32 + MAX7219 4-in-1 (32x8)
//
// Boot: display -> config -> Wi-Fi (phone setup portal) -> NTP -> fetch ->
//       parse -> show -> save -> poll every 60 s.
// Data source: https://www.thaigoldtoday.com (ข้อมูล: thaigoldtoday.com)

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "AppConfig.h"
#include "Brightness.h"
#include "GoldPriceClient.h"
#include "LedMatrix.h"
#include "OtaGuard.h"
#include "PriceStore.h"
#include "PriceTracker.h"
#include "ScreenSelector.h"
#include "Timing.h"
#include "WifiLink.h"

using namespace gold;

static constexpr int kBootButtonPin = 0;
static constexpr int64_t kClockValidAfter = 1700000000;  // 2023-11; earlier = not synced
static constexpr uint32_t kLongPressMs = 5000;
static constexpr uint32_t kModeBannerMs = 1500;

static LedMatrix display(DISPLAY_HARDWARE_TYPE, DISPLAY_DIN_PIN, DISPLAY_CLK_PIN, DISPLAY_CS_PIN,
                         DISPLAY_MODULES,
                         DISPLAY_MIRROR_X != 0, DISPLAY_FLIP_Y != 0);
static WifiLink wifi;
static constexpr ThaiPrice kPrimaryPrice =
    THAI_PRICE_ORNAMENT ? ThaiPrice::Ornament : ThaiPrice::Bar;
static ThaiGoldTodayClient apiClient(GOLD_API_URL, GOLD_API_ALLOW_INSECURE_TLS != 0,
                                     HTTP_TIMEOUT_MS, kPrimaryPrice);
static GoldPriceProvider& provider = apiClient;  // swap here to change data source
static PriceStore store;

static FreshnessPolicy makePolicy() {
    FreshnessPolicy p;
    p.maxFetchAgeMs = STALE_AFTER_MS;
    p.maxPriceAgeSec = MAX_PRICE_AGE_SEC;
    return p;
}
static PriceTracker tracker(makePolicy());

static BrightnessPolicy brightness;
static DisplayMode mode = (DisplayMode)DISPLAY_MODE_DEFAULT;
static ScreenTiming timing;
static uint32_t nextFetchMs = 0;
static uint32_t lastLogMs = 0;
static uint32_t lastDisplayRefreshMs = 0;
static OneShotTimer modeBanner;
static int currentIntensity = -1;
static bool otaStarted = false;
static bool otaInProgress = false;

// OTA is only compiled in when the build script found a valid password
// (tools/ota_secret.py defines OTA_PASSWORD_MD5). No password = no OTA.
#if OTA_ENABLED && defined(OTA_PASSWORD_MD5)
#define OTA_ACTIVE 1
#else
#define OTA_ACTIVE 0
#endif
static OtaGuardPolicy makeOtaPolicy() {
    OtaGuardPolicy p;
    p.maxFailures = OTA_MAX_AUTH_FAILURES;
    p.lockoutMs = (uint32_t)OTA_LOCKOUT_MIN * 60UL * 1000UL;
    return p;
}
static OtaGuard otaGuard(makeOtaPolicy());
static volatile bool otaStopRequested = false;
static int otaLastPercent = -1;

static int64_t epochNow() {
    const time_t t = time(nullptr);
    return (int64_t)t >= kClockValidAfter ? (int64_t)t : 0;
}

static void logPrice(const char* tag, const GoldPrice& p) {
    const time_t ts = (time_t)(p.updatedAtEpoch + UTC_OFFSET_SEC);
    struct tm tmv;
    gmtime_r(&ts, &tmv);
    Serial.printf(
        "[%s] bar buy=%ld sell=%ld | ornament buy=%ld sell=%ld | round trend=%d | day change=%ld "
        "| world=$%ld | updated %04d-%02d-%02d %02d:%02d | announced %d/%d min=%d\n",
        tag, (long)p.buyBar, (long)p.sellBar, (long)p.buyOrnament, (long)p.sellOrnament, p.trend,
        (long)p.change, (long)p.worldSpot, tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
        tmv.tm_hour, tmv.tm_min, p.announcedDay, p.announcedMonth, p.announcedMinute);
}

static void doFetch() {
    GoldPrice p;
    const FetchResult r = provider.fetch(p);
    const uint32_t nowMs = millis();  // fetch may take seconds
    if (r.ok()) {
        tracker.onFetchSuccess(p, nowMs);
        logPrice("api", tracker.price());
        if (tracker.needsPersist() && store.save(tracker.price())) {
            tracker.markPersisted();
            Serial.println("[nvs] saved last price");
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

static void showText(const char* text) {
    Frame f;
    renderCenteredText(f, text);
    display.show(f);
}

// Short press: next display mode. Hold 5 s: forget Wi-Fi, open setup portal.
static void handleButton(uint32_t nowMs) {
#if ENABLE_BOOT_BUTTON
    static bool pressed = false;
    static bool longDone = false;
    static uint32_t pressedAtMs = 0;
    static uint32_t lastEdgeMs = 0;
    const bool down = digitalRead(kBootButtonPin) == LOW;
    if (down != pressed && nowMs - lastEdgeMs > 50) {  // debounce
        lastEdgeMs = nowMs;
        pressed = down;
        if (down) {
            pressedAtMs = nowMs;
            longDone = false;
        } else if (!longDone) {
            mode = (DisplayMode)(((uint8_t)mode + 1) % kDisplayModeCount);
            store.saveMode(mode);
            modeBanner.start(nowMs, kModeBannerMs);
            Serial.printf("[ui] display mode -> %u\n", (unsigned)mode);
        }
    }
    if (pressed && !longDone && nowMs - pressedAtMs >= kLongPressMs) {
        longDone = true;
        wifi.forgetAndOpenPortal();
    }
#else
    (void)nowMs;
#endif
}

static void setupOta() {
#if OTA_ACTIVE
    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.setPasswordHash(OTA_PASSWORD_MD5);  // only the hash is in the firmware
    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        otaLastPercent = -1;
        Serial.println("[ota] authenticated, update started");
        showText("0");
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        // handle() blocks for the whole upload. Feeding only on progress means
        // a stalled upload still resets the board; the old firmware stays active.
        feedLoopWDT();
        const int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
        if (pct == otaLastPercent) return;
        otaLastPercent = pct;
        char buf[12];
        formatPrice(pct, buf, sizeof(buf));
        showText(buf);  // progress 0..100 on the display
    });
    ArduinoOTA.onEnd([]() {
        otaGuard.onSuccess();
        Serial.println("[ota] done, rebooting");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        otaInProgress = false;
        if (e == OTA_AUTH_ERROR) {
            Serial.printf("[ota] WRONG PASSWORD (%u/%u)\n", otaGuard.failures() + 1,
                          (unsigned)OTA_MAX_AUTH_FAILURES);
            if (otaGuard.onAuthFailure(millis())) otaStopRequested = true;
        } else {
            // Update library rejects bad images; the old firmware keeps running.
            Serial.printf("[ota] error %u - update discarded, old firmware kept\n", (unsigned)e);
        }
    });
    ArduinoOTA.begin();
    Serial.printf("[ota] ready: %s.local (password protected)\n", DEVICE_HOSTNAME);
#else
    Serial.println("[ota] DISABLED: no valid OTA password at build time (see README, OTA)");
#endif
}

static void serviceOta(uint32_t nowMs) {
#if OTA_ACTIVE
    if (!otaStarted) return;
    if (otaStopRequested) {
        otaStopRequested = false;
        ArduinoOTA.end();
        Serial.printf("[ota] too many wrong passwords - OTA locked for %u min\n",
                      (unsigned)OTA_LOCKOUT_MIN);
    }
    if (otaGuard.shouldUnlock(nowMs)) {
        ArduinoOTA.begin();
        Serial.println("[ota] lockout over, OTA accepting updates again");
    }
    if (!otaGuard.locked()) ArduinoOTA.handle();
#else
    (void)nowMs;
#endif
}

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.printf("\n[boot] Thai gold price LED display, reset reason=%d\n",
                  (int)esp_reset_reason());  // esp_reset_reason_t: 6 = task watchdog

    // Display first, so the user sees something immediately.
    brightness.dayIntensity = DISPLAY_INTENSITY;
    brightness.nightIntensity = NIGHT_INTENSITY;
    brightness.nightStartHour = NIGHT_START_HOUR;
    brightness.nightEndHour = NIGHT_END_HOUR;
    brightness.utcOffsetSec = UTC_OFFSET_SEC;
    if (!display.begin(DISPLAY_INTENSITY)) Serial.println("[display] init failed");
    currentIntensity = DISPLAY_INTENSITY;
    Frame frame;
    renderScreen(frame, Screen());  // "----"
    display.show(frame, true);

    // Configuration / persisted state
    pinMode(kBootButtonPin, INPUT_PULLUP);
    timing.alternatePeriodMs = ALTERNATE_PERIOD_MS;
    timing.thaiShowMs = THAI_SHOW_MS;
    timing.worldShowMs = WORLD_SHOW_MS;
    timing.dateShowMs = DATE_SHOW_MS;
    timing.timeShowMs = TIME_SHOW_MS;
    DisplayMode savedMode;
    if (store.loadMode(savedMode)) mode = savedMode;
    GoldPrice saved;
    if (store.load(saved)) {
        tracker.restore(saved);  // shown as STALE until a fresh fetch succeeds
        logPrice("nvs", saved);
    }

    // NTP (UTC). SNTP runs in the background once the network is up.
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

    // Wi-Fi: saved network, or the phone setup portal ("AP" on the display).
    wifi.begin(DEVICE_HOSTNAME, SETUP_AP_NAME, SETUP_AP_PASSWORD, WIFI_RETRY_BASE_MS,
               WIFI_RETRY_MAX_MS);

    // Reset the board if loop() ever stops returning. The Arduino core feeds
    // this after every loop() pass. The timeout must exceed the longest normal
    // blocking step, which is one HTTPS fetch.
    esp_task_wdt_init(LOOP_WATCHDOG_SEC, true);
    enableLoopWDT();
    lastDisplayRefreshMs = millis();
}

void loop() {
    const uint32_t now = millis();

    wifi.loop(now);
    tracker.setWifiConnected(wifi.connected());
    if (wifi.takeJustConnected()) {
        nextFetchMs = now;  // fetch right after (re)connect
        if (!otaStarted) {
            setupOta();
            otaStarted = true;
        }
    }
    serviceOta(now);
    if (otaInProgress) return;

    if (wifi.connected() && (int32_t)(now - nextFetchMs) >= 0) doFetch();

    handleButton(now);

    const uint32_t t = millis();
    const int64_t epoch = epochNow();
    tracker.tick(t);

    const uint8_t want = intensityFor(epoch, brightness);
    if (want != currentIntensity) {
        display.setIntensity(want);
        currentIntensity = want;
        Serial.printf("[display] intensity -> %u\n", want);
    }

    Frame frame;
    if (modeBanner.active(t)) {
        char banner[4] = {'-', (char)('0' + (uint8_t)mode), '-', '\0'};
        renderCenteredText(frame, banner);
    } else {
        ScreenInput in;
        in.hasPrice = tracker.hasPrice();
        in.price = tracker.price();
        in.fresh = tracker.isFresh(t, epoch);
        in.fault = tracker.currentFault(t, epoch);
        in.mode = mode;
        in.nowMs = t;
        in.useOrnament = THAI_PRICE_ORNAMENT != 0;
        in.showTrend = SHOW_TREND_ARROW != 0;
        in.setupPortal = wifi.portalActive() && !wifi.connected();
        renderScreen(frame, selectScreen(in, timing));
    }
    if (t - lastDisplayRefreshMs >= DISPLAY_REFRESH_MS) {
        // Recover from noise or a panel power dip without a reboot.
        lastDisplayRefreshMs = t;
        display.refreshHardware((uint8_t)currentIntensity);
        display.show(frame, true);
    } else {
        display.show(frame);
    }

    if (t - lastLogMs > 30000) {
        lastLogMs = t;
        Serial.printf("[state] wifi=%d portal=%d fresh=%d fault=E%u clock=%s mode=%u heap=%u\n",
                      wifi.connected(), wifi.portalActive(), tracker.isFresh(t, epoch),
                      (unsigned)tracker.currentFault(t, epoch), epoch ? "synced" : "unsynced",
                      (unsigned)mode, (unsigned)ESP.getFreeHeap());
    }
    delay(20);
}
