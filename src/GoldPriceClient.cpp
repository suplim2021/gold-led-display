#include "GoldPriceClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "GoldPriceParser.h"
#include "JsonEndDetector.h"
#include "RootCA.h"
#include "Timing.h"

static constexpr int kMaxBodyBytes = 32 * 1024;  // live response is ~2-5 KB

// Reads the response body under a total time and size limit. HTTPClient's own
// getString() can wait forever when a server keeps the connection open without
// sending data, because its read loop has no timeout.
static gold::TransferState readBody(HTTPClient& http, String& body, gold::TransferGuard& guard,
                                    int declared) {
    WiFiClient* stream = http.getStreamPtr();
    gold::JsonEndDetector jsonEnd;
    uint8_t buf[512];
    while (stream) {
        gold::TransferState state = guard.check(millis());
        if (state != gold::TransferState::Ok) return state;
        if (declared >= 0 && (int)guard.bytes() >= declared) break;

        const int available = stream->available();
        if (available > 0) {
            const size_t want = available < (int)sizeof(buf) ? (size_t)available : sizeof(buf);
            const int got = stream->read(buf, want);
            if (got <= 0) continue;
            state = guard.onBytes((size_t)got, millis());
            if (state != gold::TransferState::Ok) return state;
            // Out of memory: report as too large, it is handled the same way.
            if (!body.concat((const char*)buf, (unsigned int)got)) {
                return gold::TransferState::TooLarge;
            }
            // Complete JSON: do not wait for the server to close the connection.
            if (jsonEnd.feed((const char*)buf, (size_t)got)) break;
        } else if (!http.connected()) {
            break;  // server closed the connection: end of body
        } else {
            delay(5);
        }
    }
    return gold::TransferState::Ok;
}

gold::FetchResult ThaiGoldTodayClient::fetch(gold::GoldPrice& out) {
    gold::FetchResult r;
    // Connect, TLS handshake and body each get up to timeoutMs_.
    const uint32_t startMs = millis();
    gold::TransferGuard guard(startMs, timeoutMs_ * 3, kMaxBodyBytes);

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
    // Over HTTP/1.1 this API answers chunked, which readBody() does not decode.
    // Verified 2026-11-06: over HTTP/1.0 it sends the plain body, then closes.
    http.useHTTP10(true);
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
    String body;
    body.reserve(declared > 0 ? declared + 1 : 4096);
    const uint32_t headersMs = millis() - startMs;
    const gold::TransferState state = readBody(http, body, guard, declared);
    Serial.printf("[api] headers %lu ms, total %lu ms, %u bytes, last='%c', state=%u, rssi=%d\n",
                  (unsigned long)headersMs, (unsigned long)(millis() - startMs),
                  (unsigned)body.length(), body.length() ? body[body.length() - 1] : '-',
                  (unsigned)state, WiFi.RSSI());
    http.end();
    if (state != gold::TransferState::Ok) {
        r.fault = gold::Fault::Api;
        r.detail = state == gold::TransferState::TimedOut ? "response timeout" : "response too large";
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
