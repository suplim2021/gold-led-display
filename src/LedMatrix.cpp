#include "LedMatrix.h"

bool LedMatrix::begin(uint8_t intensity) {
    if (!mx_.begin()) return false;
    mx_.control(MD_MAX72XX::INTENSITY, intensity > 15 ? 15 : intensity);
    mx_.clear();
    return true;
}

void LedMatrix::setIntensity(uint8_t intensity) {
    mx_.control(MD_MAX72XX::INTENSITY, intensity > 15 ? 15 : intensity);
}

void LedMatrix::show(const gold::Frame& frame, bool force) {
    if (!force && hasShown_ && frame == shown_) return;
    const uint16_t cols = mx_.getColumnCount();
    mx_.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx_.clear();
    for (int y = 0; y < gold::Frame::kHeight; ++y) {
        for (int x = 0; x < gold::Frame::kWidth && x < cols; ++x) {
            if (!frame.get(x, y)) continue;
            // MD_MAX72XX: column 0 is the right-most column of the chain.
            const uint16_t col = mirrorX_ ? (uint16_t)x : (uint16_t)(cols - 1 - x);
            const uint8_t row = flipY_ ? (uint8_t)(gold::Frame::kHeight - 1 - y) : (uint8_t)y;
            mx_.setPoint(row, col, true);
        }
    }
    mx_.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    shown_ = frame;
    hasShown_ = true;
}
