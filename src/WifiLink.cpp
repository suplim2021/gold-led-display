#include "WifiLink.h"

#include <WiFi.h>

#include "PriceTracker.h"  // backoffDelayMs

void WifiLink::begin(const char* hostname, const char* apName, const char* apPassword,
                     uint32_t retryBaseMs, uint32_t retryMaxMs) {
    apName_ = apName;
    apPassword_ = apPassword;
    retryBaseMs_ = retryBaseMs;
    retryMaxMs_ = retryMaxMs;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.setAutoReconnect(true);

    wm_.setConfigPortalBlocking(false);
    wm_.setConnectTimeout(20);
    wm_.setHostname(hostname);
    wm_.setTitle("Gold LED");
    wm_.setShowInfoUpdate(false);  // only hides the button, see blockFirmwareUpload()
    wm_.setWebServerCallback([this]() { blockFirmwareUpload(); });
    wm_.setConfigPortalTimeout(0);  // the portal only opens when setup is needed

    const bool saved = wm_.getWiFiIsSaved();
    Serial.printf("[wifi] saved network: %s\n", saved ? "yes" : "no");
    if (saved) {
        WiFi.begin();  // never opens the portal; loop() keeps retrying
    } else if (!wm_.autoConnect(apName_, apPassword_)) {
        Serial.printf("[wifi] setup portal open: AP '%s'\n", apName_);
    }
    nextAttemptMs_ = millis() + retryBaseMs_;
}

// WiFiManager always registers its firmware upload routes, and they do not ask
// for the OTA password. WebServer uses the first handler that matches, and this
// callback runs before WiFiManager adds its own, so these take over both routes.
// No upload callback is passed, so uploaded data never reaches the Update code.
void WifiLink::blockFirmwareUpload() {
    const auto reject = [this]() { wm_.server->send(404, "text/plain", "Not found"); };
    wm_.server->on(WM_G(R_update), HTTP_ANY, reject);
    wm_.server->on(WM_G(R_updatedone), HTTP_ANY, reject);
}

void WifiLink::openPortal() {
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
