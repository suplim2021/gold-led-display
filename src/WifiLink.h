// Wi-Fi connection: setup through a phone (captive portal, tzapu/WiFiManager)
// plus non-blocking reconnect with exponential backoff.
//
// First boot (no saved network) -> access point "GoldLED-Setup" opens and the
// display shows "AP". Connect with a phone, pick your Wi-Fi, enter password.
// Holding the BOOT button 5 s forgets the network and opens the portal again.
#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

class WifiLink {
public:
    void begin(const char* hostname, const char* apName, const char* apPassword,
               uint32_t retryBaseMs, uint32_t retryMaxMs, uint32_t portalTimeoutSec);
    void loop(uint32_t nowMs);  // never blocks
    bool connected() const { return connected_; }
    bool portalActive() { return wm_.getConfigPortalActive(); }
    bool takeJustConnected();
    void forgetAndOpenPortal();  // BOOT long press

private:
    void openPortal();

    WiFiManager wm_;
    const char* apName_ = "";
    const char* apPassword_ = "";
    uint32_t retryBaseMs_ = 10000;
    uint32_t retryMaxMs_ = 120000;
    uint32_t portalTimeoutSec_ = 180;
    uint32_t nextAttemptMs_ = 0;
    uint8_t attempt_ = 0;
    bool connected_ = false;
    bool justConnected_ = false;
};
