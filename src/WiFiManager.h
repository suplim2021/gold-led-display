// Non-blocking Wi-Fi station manager with exponential reconnect backoff.
#pragma once

#include <Arduino.h>

class WiFiManager {
public:
    void begin(const char* ssid, const char* password, const char* hostname,
               uint32_t retryBaseMs, uint32_t retryMaxMs);
    // Call often from loop(). Never blocks.
    void loop(uint32_t nowMs);
    bool connected() const { return connected_; }
    // True once per transition to connected (consumed by the caller).
    bool takeJustConnected();

private:
    const char* ssid_ = "";
    const char* password_ = "";
    uint32_t retryBaseMs_ = 10000;
    uint32_t retryMaxMs_ = 120000;
    uint32_t nextAttemptMs_ = 0;
    uint8_t attempt_ = 0;
    bool connected_ = false;
    bool justConnected_ = false;
};
