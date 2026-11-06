#pragma once

// The ESP32 SDK already ships headers named "config.h", hence the unique name.
#if __has_include("gold_config.h")
#include "gold_config.h"
#else
#warning "include/gold_config.h not found - using gold_config.example.h . Copy include/gold_config.example.h to include/gold_config.h"
#include "gold_config.example.h"
#endif

// Defaults for settings added after gold_config.h may have been copied.
#ifndef THAI_PRICE_ORNAMENT
#define THAI_PRICE_ORNAMENT 1
#endif
#ifndef OTA_ENABLED
#define OTA_ENABLED 1
#endif
#ifndef OTA_MAX_AUTH_FAILURES
#define OTA_MAX_AUTH_FAILURES 5
#endif
#ifndef OTA_LOCKOUT_MIN
#define OTA_LOCKOUT_MIN 15
#endif
#ifndef DISPLAY_DIN_PIN
#define DISPLAY_DIN_PIN 23
#endif
#ifndef DISPLAY_CLK_PIN
#define DISPLAY_CLK_PIN 18
#endif
