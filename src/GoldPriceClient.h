// HTTPS client for https://www.thaigoldtoday.com/api/gold-price
// (implementation of gold::GoldPriceProvider for the ESP32).
#pragma once

#include "GoldPriceParser.h"
#include "GoldPriceProvider.h"

class ThaiGoldTodayClient : public gold::GoldPriceProvider {
public:
    ThaiGoldTodayClient(const char* url, bool allowInsecureTls, uint32_t timeoutMs,
                        gold::ThaiPrice primary)
        : url_(url), insecure_(allowInsecureTls), timeoutMs_(timeoutMs), primary_(primary) {}
    const char* name() const override { return "thaigoldtoday.com"; }
    gold::FetchResult fetch(gold::GoldPrice& out) override;

private:
    const char* url_;
    bool insecure_;
    uint32_t timeoutMs_;
    gold::ThaiPrice primary_;
};
