// Pushes a gold::Frame to the MAX7219 chain via MD_MAX72XX.
#pragma once

#include <MD_MAX72xx.h>

#include "Frame.h"

class LedMatrix {
public:
    LedMatrix(MD_MAX72XX::moduleType_t type, uint8_t csPin, uint8_t modules, bool mirrorX,
              bool flipY)
        : mx_(type, csPin, modules), mirrorX_(mirrorX), flipY_(flipY) {}

    bool begin(uint8_t intensity);
    void setIntensity(uint8_t intensity);
    // Sends the frame only if it differs from what is already displayed.
    void show(const gold::Frame& frame, bool force = false);

private:
    MD_MAX72XX mx_;
    bool mirrorX_;
    bool flipY_;
    gold::Frame shown_;
    bool hasShown_ = false;
};
