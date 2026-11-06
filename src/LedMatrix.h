// Pushes a gold::Frame to the MAX7219 chain via MD_MAX72XX.
#pragma once

#include <MD_MAX72xx.h>

#include "Frame.h"

class LedMatrix {
public:
    // Software (bit-banged) SPI on the given pins. MD_MAX72XX's hardware SPI
    // runs at a fixed 8 MHz, which leaves little margin when the 5 V MAX7219
    // is driven directly by 3.3 V GPIO over jumper wires. The display only
    // changes a few times per second, so the slower clock costs nothing.
    LedMatrix(MD_MAX72XX::moduleType_t type, uint8_t dinPin, uint8_t clkPin, uint8_t csPin,
              uint8_t modules, bool mirrorX, bool flipY)
        : mx_(type, dinPin, clkPin, csPin, modules), mirrorX_(mirrorX), flipY_(flipY) {}

    bool begin(uint8_t intensity);
    void setIntensity(uint8_t intensity);
    // Rewrites every MAX7219 config register. They are otherwise written only
    // once, so a glitch could leave the panel blank until the next reboot.
    // Follow with show(frame, true) to rewrite the pixels too.
    void refreshHardware(uint8_t intensity);
    // Sends the frame only if it differs from what is already displayed.
    void show(const gold::Frame& frame, bool force = false);

private:
    MD_MAX72XX mx_;
    bool mirrorX_;
    bool flipY_;
    gold::Frame shown_;
    bool hasShown_ = false;
};
