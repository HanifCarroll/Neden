#include "motion_preferences.h"

struct MotionPreferences::NativeState {};
MotionPreferences::MotionPreferences(QObject *parent)
    : QObject(parent), native(std::make_unique<NativeState>()) {}
MotionPreferences::~MotionPreferences() = default;
bool MotionPreferences::reducedMotion() const {
    // Animate only on platforms where the accessibility preference is known.
    return true;
}
