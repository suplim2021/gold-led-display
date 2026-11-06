// Hardware-independent timing helpers that stay correct when millis() wraps.
//
// A device that runs for months must never compare a stored deadline with
// (int32_t)(deadline - now): once the deadline is more than 24.8 days old the
// signed result flips and it looks like it is in the future again. These
// helpers keep explicit state and only ever measure elapsed unsigned time.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gold {

// Runs for a duration after start(), then stays off until started again.
// Poll active() at least once per 49 days so that expiry is latched.
class OneShotTimer {
public:
    void start(uint32_t nowMs, uint32_t durationMs) {
        startedAtMs_ = nowMs;
        durationMs_ = durationMs;
        running_ = true;
    }

    bool active(uint32_t nowMs) {
        if (!running_) return false;
        if ((uint32_t)(nowMs - startedAtMs_) >= durationMs_) running_ = false;
        return running_;
    }

private:
    uint32_t startedAtMs_ = 0;
    uint32_t durationMs_ = 0;
    bool running_ = false;
};

enum class TransferState : uint8_t { Ok, TooLarge, TimedOut };

// Limits a whole download by total time and total bytes, so neither a stalled
// connection nor a slow trickle of data can hold the main loop for long.
class TransferGuard {
public:
    TransferGuard(uint32_t startMs, uint32_t maxMs, size_t maxBytes)
        : startMs_(startMs), maxMs_(maxMs), maxBytes_(maxBytes) {}

    TransferState check(uint32_t nowMs) const {
        if (bytes_ > maxBytes_) return TransferState::TooLarge;
        if ((uint32_t)(nowMs - startMs_) >= maxMs_) return TransferState::TimedOut;
        return TransferState::Ok;
    }

    TransferState onBytes(size_t count, uint32_t nowMs) {
        bytes_ += count;
        return check(nowMs);
    }

    size_t bytes() const { return bytes_; }

private:
    uint32_t startMs_;
    uint32_t maxMs_;
    size_t maxBytes_;
    size_t bytes_ = 0;
};

}  // namespace gold
