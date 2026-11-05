#include "WiFiManager.h"

#include <WiFi.h>

#include "PriceTracker.h"  // backoffDelayMs

void WiFiManager::begin(const char* ssid, const char* password, const char* hostname,
                        uint32_t retryBaseMs, uint32_t retryMaxMs) {
    ssid_ = ssid;
    password_ = password;
    retryBaseMs_ = retryBaseMs;
    retryMaxMs_ = retryMaxMs;
    WiFi.persistent(false);  // do not write credentials to flash on every begin()
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(true);
    Serial.printf("[wifi] connecting to '%s'\n", ssid_);
    WiFi.begin(ssid_, password_);
    nextAttemptMs_ = millis() + retryBaseMs_;
}

bool WiFiManager::takeJustConnected() {
    const bool r = justConnected_;
    justConnected_ = false;
    return r;
}

void WiFiManager::loop(uint32_t nowMs) {
    const bool up = WiFi.status() == WL_CONNECTED;
    if (up) {
        if (!connected_) {
            Serial.printf("[wifi] connected, ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            justConnected_ = true;
        }
        connected_ = true;
        attempt_ = 0;
        return;
    }
    if (connected_) {
        Serial.println("[wifi] connection lost");
        connected_ = false;
        nextAttemptMs_ = nowMs + retryBaseMs_;  // give auto-reconnect a chance first
        return;
    }
    if ((int32_t)(nowMs - nextAttemptMs_) < 0) return;

    if (attempt_ < 255) ++attempt_;
    const uint32_t wait = gold::backoffDelayMs(attempt_, retryBaseMs_, retryMaxMs_);
    Serial.printf("[wifi] retry #%u (next in %lus)\n", attempt_, (unsigned long)(wait / 1000));
    WiFi.disconnect(false, false);
    WiFi.begin(ssid_, password_);
    nextAttemptMs_ = nowMs + wait;
}
