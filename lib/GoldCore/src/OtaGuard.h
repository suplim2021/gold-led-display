// Brute-force protection for over-the-air updates.
//
// ArduinoOTA authenticates with a challenge/response on the password hash, but
// it lets a client retry forever. OtaGuard counts failed authentications and
// asks the firmware to stop the OTA service for `lockoutMs` after
// `maxFailures` failures in a row. A successful update resets the counter.
#pragma once

#include <stdint.h>

namespace gold {

struct OtaGuardPolicy {
    uint8_t maxFailures = 5;
    uint32_t lockoutMs = 15UL * 60UL * 1000UL;
};

class OtaGuard {
public:
    explicit OtaGuard(const OtaGuardPolicy& p = OtaGuardPolicy()) : policy_(p) {}

    // Returns true if this failure starts a lockout (caller stops OTA).
    bool onAuthFailure(uint32_t nowMs);
    void onSuccess() { failures_ = 0; }

    bool locked() const { return locked_; }
    // Returns true once when the lockout has expired (caller restarts OTA).
    bool shouldUnlock(uint32_t nowMs);
    uint8_t failures() const { return failures_; }
    uint32_t remainingMs(uint32_t nowMs) const;

private:
    OtaGuardPolicy policy_;
    uint8_t failures_ = 0;
    bool locked_ = false;
    uint32_t lockedAtMs_ = 0;
};

// Password policy used by the build script and documented for the user.
// Returns nullptr if acceptable, else a short reason.
const char* otaPasswordProblem(const char* password);

}  // namespace gold
