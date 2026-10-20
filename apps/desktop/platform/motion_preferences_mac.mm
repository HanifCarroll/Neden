#include "motion_preferences.h"
#include <QPointer>
#import <AppKit/AppKit.h>

struct MotionPreferences::NativeState {
    id observer = nil;
};
MotionPreferences::MotionPreferences(QObject *parent)
    : QObject(parent), native(std::make_unique<NativeState>()) {
    const QPointer<MotionPreferences> preferences(this);
    native->observer = [NSWorkspace.sharedWorkspace.notificationCenter
        addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
        object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *) {
            if (preferences)
                emit preferences->reducedMotionChanged();
        }];
}
MotionPreferences::~MotionPreferences() {
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:native->observer];
}
bool MotionPreferences::reducedMotion() const {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}
