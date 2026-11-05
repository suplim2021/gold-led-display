// ---------------------------------------------------------------------------
// Configuration template.
//
// Copy this file to include/gold_config.h and edit it. include/gold_config.h is in
// .gitignore so your Wi-Fi password is never committed. If gold_config.h does not
// exist the firmware still builds with these defaults (but cannot join Wi-Fi).
// ---------------------------------------------------------------------------
#pragma once

// ----- Wi-Fi (2.4 GHz only; ESP32 does not support 5 GHz) -----
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define DEVICE_HOSTNAME "gold-led"

// ----- Display wiring (ESP32 VSPI hardware SPI) -----
// DIN -> GPIO23 (MOSI) and CLK -> GPIO18 (SCK) are fixed by hardware SPI.
#define DISPLAY_CS_PIN 5
#define DISPLAY_MODULES 4

// MD_MAX72XX hardware type. Most red "4-in-1" modules are FC16_HW.
// If characters look scrambled/mirrored try GENERIC_HW, PAROLA_HW or ICSTATION_HW.
#define DISPLAY_HARDWARE_TYPE MD_MAX72XX::FC16_HW

// Fine tuning if the picture is mirrored or upside down (0 or 1).
#define DISPLAY_MIRROR_X 0
#define DISPLAY_FLIP_Y 0

// Brightness 0 (dim) .. 15 (max). Start low: less current, less heat.
#define DISPLAY_INTENSITY 1

// ----- Display mode -----
// 0 = Mode A: sell price only (default)
// 1 = Mode B: alternate sell / buy every ALTERNATE_PERIOD_MS
// The BOOT button (GPIO0) toggles the mode at runtime; the choice is saved.
#define DISPLAY_MODE_DEFAULT 0
#define ALTERNATE_PERIOD_MS 5000
#define ENABLE_BOOT_BUTTON_TOGGLE 1

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

// Data older than this (since last successful fetch) is marked stale.
#define STALE_AFTER_MS 180000UL
// Announcement time older than this is marked stale (E4). Generous because the
// gold association does not update on weekends / public holidays.
#define MAX_PRICE_AGE_SEC (4L * 24L * 3600L)

// ----- Clock (UTC is used internally; only for staleness checks) -----
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
