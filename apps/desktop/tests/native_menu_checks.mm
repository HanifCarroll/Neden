#import <AppKit/AppKit.h>

bool nativeMenusAreAvailable() {
    for (NSString *title in @[ @"File", @"Playback", @"Subtitles", @"Study", @"View" ]) {
        NSMenuItem *item = [NSApp.mainMenu itemWithTitle:title];
        if (!item || item.submenu.numberOfItems == 0)
            return false;
    }
    return true;
}

#include <QQuickWindow>

bool nativeVideoExtendsUnderTitlebar(QQuickWindow *window) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    return (view.window.styleMask & NSWindowStyleMaskFullSizeContentView) &&
           view.window.titleVisibility == NSWindowTitleHidden;
}
