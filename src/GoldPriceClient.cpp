#include "GoldPriceClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "GoldPriceParser.h"
#include "RootCA.h"

static constexpr int kMaxBodyBytes = 32 * 1024;  // live response is ~4.5 KB

gold::FetchResult ThaiGoldTodayClient::fetch(gold::GoldPrice& out) {
    gold::FetchResult r;

    WiFiClientSecure tls;
    if (insecure_) {
        tls.setInsecure();
    } else {
        tls.setCACert(GOLD_API_ROOT_CA);
    }
    const uint32_t timeoutSec = (timeoutMs_ + 999) / 1000;
    tls.setTimeout(timeoutSec);           // socket read/write, seconds
    tls.setHandshakeTimeout(timeoutSec);  // default would be 120 s

    HTTPClient http;
    http.setConnectTimeout(timeoutMs_);
    http.setTimeout(timeoutMs_);
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("gold-led-display/1.0 (ESP32)");
    if (!http.begin(tls, url_)) {
        r.fault = gold::Fault::Api;
        r.detail = "http.begin failed";
        return r;
    }
    http.addHeader("Accept", "application/json");

    const int status = http.GET();
    r.httpStatus = status;
    if (status != 200) {
        r.fault = gold::faultForHttpStatus(status);
        r.detail = status < 0 ? "transport error/timeout" : "http status";
        Serial.printf("[api] GET failed: %d %s\n", status,
                      status < 0 ? HTTPClient::errorToString(status).c_str() : "");
        http.end();
        return r;
    }
    const int declared = http.getSize();
    if (declared > kMaxBodyBytes) {
        http.end();
        r.fault = gold::Fault::Api;
        r.detail = "response too large";
        return r;
    }
    const String body = http.getString();
    http.end();
    if ((int)body.length() > kMaxBodyBytes) {
        r.fault = gold::Fault::Api;
        r.detail = "response too large";
        return r;
    }

    gold::GoldPrice parsed;
    const gold::ParseError pe = gold::parseThaiGoldToday(body.c_str(), body.length(), parsed, primary_);
    if (pe != gold::ParseError::Ok) {
        r.fault = pe == gold::ParseError::EmptyBody ? gold::Fault::Api : gold::Fault::Data;
        r.detail = gold::parseErrorName(pe);
        Serial.printf("[api] parse error: %s (%u bytes)\n", r.detail, (unsigned)body.length());
        return r;
    }
    out = parsed;
    r.detail = "ok";
    return r;
}
