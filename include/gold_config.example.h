// ---------------------------------------------------------------------------
// Configuration template.
//
// Optional: copy this file to include/gold_config.h and edit it. Without
// gold_config.h the firmware builds with these defaults.
// Wi-Fi is NOT configured here any more: set it from your phone (see README).
// ---------------------------------------------------------------------------
#pragma once

// ----- Wi-Fi setup portal (phone) -----
#define DEVICE_HOSTNAME "gold-led"          // also the OTA name: gold-led.local
#define SETUP_AP_NAME "GoldLED-Setup"       // Wi-Fi name shown on your phone
#define SETUP_AP_PASSWORD "goldprice"       // 8+ chars, or "" for an open AP

// ----- Over-the-air update (Wi-Fi) -----
// The password is NOT set here: put it in ota_password.txt (project root,
// git-ignored) or the GOLD_OTA_PASSWORD environment variable. Without a valid
// password (>= 10 chars) the firmware is built with OTA switched off.
#define OTA_ENABLED 1
#define OTA_MAX_AUTH_FAILURES 5             // wrong passwords in a row ...
#define OTA_LOCKOUT_MIN 15                  // ... lock OTA for this long

// ----- Display wiring -----
// Driven with slow software SPI (more reliable with 3.3 V logic), so any
// output-capable GPIO works. Defaults match docs/WIRING.md.
#define DISPLAY_DIN_PIN 23
#define DISPLAY_CLK_PIN 18
#define DISPLAY_CS_PIN 5
#define DISPLAY_MODULES 4

// MD_MAX72XX hardware type. Most red "4-in-1" modules are FC16_HW.
// If characters look scrambled/mirrored try GENERIC_HW, PAROLA_HW or ICSTATION_HW.
#define DISPLAY_HARDWARE_TYPE MD_MAX72XX::FC16_HW

// Fine tuning if the picture is mirrored or upside down (0 or 1).
#define DISPLAY_MIRROR_X 0
#define DISPLAY_FLIP_Y 0

// ----- Brightness 0 (dimmest, still on) .. 15 (max) -----
#define DISPLAY_INTENSITY 1          // daytime
#define NIGHT_INTENSITY 0            // night: dimmest possible
#define NIGHT_START_HOUR 22          // Thai time, 22:00
#define NIGHT_END_HOUR 6             // Thai time, 06:00
#define UTC_OFFSET_SEC (7 * 3600)    // Thailand

// ----- What to show -----
// 0 = Thai sell price only
// 1 = Thai sell / buy alternating (ALTERNATE_PERIOD_MS each)
// 2 = Thai sell (THAI_SHOW_MS) / world spot "$4132" (WORLD_SHOW_MS) /
//     announcement date "6/11" (DATE_SHOW_MS) / time "10:46" (TIME_SHOW_MS)   <- default
// Short press of the BOOT button cycles 0 -> 1 -> 2; the choice is saved.
#define DISPLAY_MODE_DEFAULT 2
#define ALTERNATE_PERIOD_MS 5000
#define THAI_SHOW_MS 10000
#define WORLD_SHOW_MS 4000
#define DATE_SHOW_MS 2000
#define TIME_SHOW_MS 2000
// Main Thai price: 1 = ทองรูปพรรณ 96.5% ขายออก (sellOrnament)  <- default
//                  0 = ทองคำแท่ง 96.5% ขายออก (sellBar)
// The chosen price is required: if the API stops sending it the display shows
// E3 instead of silently falling back to the other price.
#define THAI_PRICE_ORNAMENT 1
// Trend triangle: top-right = last round went up, bottom-right = down.
#define SHOW_TREND_ARROW 1
#define ENABLE_BOOT_BUTTON 1

// ----- Gold price API -----
#define GOLD_API_URL "https://www.thaigoldtoday.com/api/gold-price"
// 1 = skip TLS certificate verification (only if the bundled root CAs ever
// stop matching the server). Keep 0 normally.
#define GOLD_API_ALLOW_INSECURE_TLS 0
#define HTTP_TIMEOUT_MS 10000

// ----- Timing -----
#define POLL_INTERVAL_MS 60000UL         // API caches for 60 s; do not go lower
#define RETRY_BASE_MS 15000UL            // first retry after an error
#define RETRY_MAX_MS 300000UL            // retries back off up to 5 min
#define WIFI_RETRY_BASE_MS 10000UL
#define WIFI_RETRY_MAX_MS 120000UL
#define LOOP_WATCHDOG_SEC 90             // reset if loop() hangs this long; keep above 3 x HTTP_TIMEOUT_MS
#define DISPLAY_REFRESH_MS 60000UL       // rewrite MAX7219 registers and pixels this often

// Data older than this (since last successful fetch) is marked stale.
#define STALE_AFTER_MS 180000UL
// Announcement time older than this is marked stale (E4). Generous because the
// gold association does not update on weekends / public holidays.
#define MAX_PRICE_AGE_SEC (4L * 24L * 3600L)

// ----- Clock (only for staleness checks and night dimming) -----
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
