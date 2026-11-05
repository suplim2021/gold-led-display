#include "PriceStore.h"

#include <Preferences.h>

static const char* kNs = "goldled";
static constexpr uint8_t kVersion = 1;

bool PriceStore::load(gold::GoldPrice& out) {
    Preferences prefs;
    if (!prefs.begin(kNs, true)) return false;
    bool ok = prefs.getUChar("ver", 0) == kVersion;
    if (ok) {
        gold::GoldPrice p;
        p.buyBar = prefs.getInt("buy", 0);
        p.sellBar = prefs.getInt("sell", 0);
        p.change = prefs.getInt("chg", 0);
        p.hasChange = prefs.getBool("hasChg", false);
        p.updatedAtEpoch = prefs.getLong64("upd", 0);
        ok = p.sellBar > 0 && p.buyBar > 0 && p.updatedAtEpoch > 0;
        if (ok) out = p;
    }
    prefs.end();
    return ok;
}

bool PriceStore::save(const gold::GoldPrice& p) {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) return false;
    prefs.putInt("buy", p.buyBar);
    prefs.putInt("sell", p.sellBar);
    prefs.putInt("chg", p.change);
    prefs.putBool("hasChg", p.hasChange);
    prefs.putLong64("upd", p.updatedAtEpoch);
    prefs.putUChar("ver", kVersion);
    prefs.end();
    return true;
}

bool PriceStore::loadMode(gold::DisplayMode& out) {
    Preferences prefs;
    if (!prefs.begin(kNs, true)) return false;
    const uint8_t m = prefs.getUChar("mode", 0xFF);
    prefs.end();
    if (m > 1) return false;
    out = (gold::DisplayMode)m;
    return true;
}

void PriceStore::saveMode(gold::DisplayMode m) {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) return;
    prefs.putUChar("mode", (uint8_t)m);
    prefs.end();
}
