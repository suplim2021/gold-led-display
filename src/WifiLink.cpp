#include "WifiLink.h"

#include <WiFi.h>

#include "PriceTracker.h"  // backoffDelayMs

void WifiLink::begin(const char* hostname, const char* apName, const char* apPassword,
                     uint32_t retryBaseMs, uint32_t retryMaxMs, uint32_t portalTimeoutSec) {
    apName_ = apName;
    apPassword_ = apPassword;
    retryBaseMs_ = retryBaseMs;
    retryMaxMs_ = retryMaxMs;
    portalTimeoutSec_ = portalTimeoutSec;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.setAutoReconnect(true);

    wm_.setConfigPortalBlocking(false);
    wm_.setConnectTimeout(20);
    wm_.setHostname(hostname);
    wm_.setTitle("Gold LED");
    wm_.setShowInfoUpdate(false);  // no firmware upload through the portal page

    const bool saved = wm_.getWiFiIsSaved();
    // With a saved network the portal only stays open for a while (e.g. router
    // was off at boot); without one it waits until you configure it.
    wm_.setConfigPortalTimeout(saved ? portalTimeoutSec_ : 0);
    Serial.printf("[wifi] saved network: %s\n", saved ? "yes" : "no");
    if (wm_.autoConnect(apName_, apPassword_)) {
        Serial.println("[wifi] connected");
    } else {
        Serial.printf("[wifi] setup portal open: AP '%s'\n", apName_);
    }
    nextAttemptMs_ = millis() + retryBaseMs_;
}

void WifiLink::openPortal() {
    wm_.setConfigPortalTimeout(wm_.getWiFiIsSaved() ? portalTimeoutSec_ : 0);
    wm_.startConfigPortal(apName_, apPassword_);
    Serial.printf("[wifi] setup portal open: AP '%s'\n", apName_);
}

void WifiLink::forgetAndOpenPortal() {
    Serial.println("[wifi] forgetting saved network");
    wm_.resetSettings();
    connected_ = false;
    openPortal();
}

bool WifiLink::takeJustConnected() {
    const bool r = justConnected_;
    justConnected_ = false;
    return r;
}

void WifiLink::loop(uint32_t nowMs) {
    wm_.process();  // serves the portal when it is open

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
        nextAttemptMs_ = nowMs + retryBaseMs_;  // let auto-reconnect try first
        return;
    }
    if (wm_.getConfigPortalActive()) return;  // the portal handles connecting
    if (!wm_.getWiFiIsSaved()) {
        openPortal();
        return;
    }
    if ((int32_t)(nowMs - nextAttemptMs_) < 0) return;

    if (attempt_ < 255) ++attempt_;
    const uint32_t wait = gold::backoffDelayMs(attempt_, retryBaseMs_, retryMaxMs_);
    Serial.printf("[wifi] retry #%u (next in %lus)\n", attempt_, (unsigned long)(wait / 1000));
    WiFi.disconnect(false, false);
    WiFi.begin();  // saved credentials
    nextAttemptMs_ = nowMs + wait;
}
