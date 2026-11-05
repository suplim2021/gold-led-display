// Persists the last valid price in NVS (flash) so it survives power loss.
// Writes happen only when the price actually changed.
#pragma once

#include "GoldPrice.h"
#include "ScreenSelector.h"

class PriceStore {
public:
    bool load(gold::GoldPrice& out);
    bool save(const gold::GoldPrice& p);
    bool loadMode(gold::DisplayMode& out);
    void saveMode(gold::DisplayMode m);
};
