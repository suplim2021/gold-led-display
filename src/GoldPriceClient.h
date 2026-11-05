// HTTPS client for https://www.thaigoldtoday.com/api/gold-price
// (implementation of gold::GoldPriceProvider for the ESP32).
#pragma once

#include "GoldPriceProvider.h"

class ThaiGoldTodayClient : public gold::GoldPriceProvider {
public:
    ThaiGoldTodayClient(const char* url, bool allowInsecureTls, uint32_t timeoutMs)
        : url_(url), insecure_(allowInsecureTls), timeoutMs_(timeoutMs) {}
    const char* name() const override { return "thaigoldtoday.com"; }
    gold::FetchResult fetch(gold::GoldPrice& out) override;

private:
    const char* url_;
    bool insecure_;
    uint32_t timeoutMs_;
};
