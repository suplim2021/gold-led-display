#include "OtaGuard.h"

#include <ctype.h>
#include <string.h>

namespace gold {

bool OtaGuard::onAuthFailure(uint32_t nowMs) {
    if (locked_) return false;
    if (failures_ < 255) ++failures_;
    if (failures_ >= policy_.maxFailures) {
        locked_ = true;
        lockedAtMs_ = nowMs;
        return true;
    }
    return false;
}

bool OtaGuard::shouldUnlock(uint32_t nowMs) {
    if (!locked_) return false;
    if ((uint32_t)(nowMs - lockedAtMs_) < policy_.lockoutMs) return false;
    locked_ = false;
    failures_ = 0;
    return true;
}

uint32_t OtaGuard::remainingMs(uint32_t nowMs) const {
    if (!locked_) return 0;
    const uint32_t elapsed = nowMs - lockedAtMs_;
    return elapsed >= policy_.lockoutMs ? 0 : policy_.lockoutMs - elapsed;
}

const char* otaPasswordProblem(const char* pw) {
    if (!pw || !*pw) return "empty";
    const size_t n = strlen(pw);
    if (n < 10) return "shorter than 10 characters";
    if (n > 64) return "longer than 64 characters";
    static const char* const kBanned[] = {"goldled-ota", "goldprice", "password", "1234567890",
                                          "0123456789", "qwertyuiop", "gold-led-display"};
    for (const char* b : kBanned) {
        if (strlen(b) != n) continue;
        bool same = true;
        for (size_t i = 0; i < n && same; ++i) same = tolower((unsigned char)pw[i]) == b[i];
        if (same) return "a well-known default";
    }
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = (unsigned char)pw[i];
        if (c < 0x21 || c > 0x7e || c == '"' || c == '\\') return "contains spaces, quotes, backslash or non-ASCII";
    }
    return nullptr;
}

}  // namespace gold
