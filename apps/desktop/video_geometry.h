#pragma once
#include <QSize>
#include <stdexcept>

inline QSize rotatedVideoSize(QSize displayPixels, int rotation) {
    if (displayPixels.isEmpty() || (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)) {
        throw std::invalid_argument("Invalid video dimensions or rotation.");
    }

    return rotation == 90 || rotation == 270 ? displayPixels.transposed() : displayPixels;
}
