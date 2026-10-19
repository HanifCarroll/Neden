#import <AppKit/AppKit.h>

bool nativeMenusAreAvailable() {
    for (NSString *title in @[@"File", @"Playback", @"Subtitles", @"Study", @"View"]) {
        NSMenuItem *item = [NSApp.mainMenu itemWithTitle:title];
        if (!item || item.submenu.numberOfItems == 0) return false;
    }
    return true;
}

#include <QQuickWindow>

bool nativeInspectorControlsVisible(QQuickWindow *window) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if (!(view.window.styleMask & NSWindowStyleMaskTitled)) return false;

    NSView *accessory = view.window.titlebarAccessoryViewControllers.lastObject.view;
    return accessory && !accessory.isHidden;
}

bool clickNativeInspectorButton(QQuickWindow *window, unsigned index) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if (!(view.window.styleMask & NSWindowStyleMaskTitled)) return false;

    NSView *accessory = view.window.titlebarAccessoryViewControllers.lastObject.view;
    if (index >= accessory.subviews.count) return false;

    NSView *item = accessory.subviews[index];
    if (![item isKindOfClass:[NSButton class]]) return false;

    [(NSButton *)item performClick:nil];
    return true;
}
