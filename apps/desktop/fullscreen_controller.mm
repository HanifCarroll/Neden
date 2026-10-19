#include "fullscreen_controller.h"
#include <QScreen>
#include <QSettings>
#import <AppKit/AppKit.h>

@interface NedenFullscreenTarget : NSObject
@property(nonatomic, assign) FullscreenController *controller;
- (void)toggleFullScreen:(id)sender;
- (void)openSettings:(id)sender;
- (void)showTranscript:(id)sender;
- (void)showMeaning:(id)sender;
@end

@implementation NedenFullscreenTarget
- (void)showTranscript:(id)sender {
    Q_UNUSED(sender);
    if (self.controller) emit self.controller->transcriptRequested();
}
- (void)showMeaning:(id)sender {
    Q_UNUSED(sender);
    if (self.controller) emit self.controller->meaningRequested();
}
- (void)openSettings:(id)sender {
    Q_UNUSED(sender);
    if (self.controller) emit self.controller->settingsRequested();
}
- (void)toggleFullScreen:(id)sender {
    Q_UNUSED(sender);
    if (self.controller) self.controller->toggle();
}
@end

struct FullscreenController::NativeState {
    NSApplicationPresentationOptions savedPresentation = NSApplicationPresentationDefault;
    __weak NSButton *zoomButton = nil;
    __weak id savedTarget = nil;
    SEL savedAction = nullptr;
    NedenFullscreenTarget *target = nil;
    NSMenuItem *settingsMenuItem = nil;
    NSMenuItem *viewMenuItem = nil;
    bool ownsViewMenu = false;
    NSMenuItem *fullscreenMenuItem = nil;
    NSTitlebarAccessoryViewController *inspectorAccessory = nil;
    NSMutableArray *observers = [NSMutableArray new];
};

FullscreenController::FullscreenController(QObject *parent)
    : QObject(parent), native(std::make_unique<NativeState>()) {
    preferredNativeMode = QSettings("Neden", "QtPlayer").value("nativeFullscreen", false).toBool();
}

void FullscreenController::setNativeMode(bool enabled) {
    if (preferredNativeMode == enabled) return;

    preferredNativeMode = enabled;
    QSettings("Neden", "QtPlayer").setValue("nativeFullscreen", enabled);
    if (window && !fullscreenActive && !transitioning) bindZoomButton();
    emit nativeModeChanged();
}

FullscreenController::~FullscreenController() {
    [native->settingsMenuItem.menu removeItem:native->settingsMenuItem];
    [native->fullscreenMenuItem.menu removeItem:native->fullscreenMenuItem];
    if (native->ownsViewMenu) [native->viewMenuItem.menu removeItem:native->viewMenuItem];
    for (id observer in native->observers) [[NSNotificationCenter defaultCenter] removeObserver:observer];
    native->target.controller = nullptr;
    NSView *view = window ? (__bridge NSView *)reinterpret_cast<void *>(window->winId()) : nil;
    if (view.window.styleMask & NSWindowStyleMaskTitled) {
        const NSUInteger accessoryIndex = [view.window.titlebarAccessoryViewControllers indexOfObject:native->inspectorAccessory];
        if (accessoryIndex != NSNotFound) [view.window removeTitlebarAccessoryViewControllerAtIndex:accessoryIndex];
    }
    if (fullscreenActive && !nativeSession) {
        NSApp.presentationOptions = native->savedPresentation;
    }

    if (native->zoomButton.target == native->target) {
        native->zoomButton.target = native->savedTarget;
        native->zoomButton.action = native->savedAction;
    }
}

void FullscreenController::attach(QQuickWindow *targetWindow) {
    if (window) {
        return;
    }

    // 1. Route the native window button through the shared fullscreen controller.
    window = targetWindow;
    native->target = [NedenFullscreenTarget new];
    native->target.controller = this;
    bindZoomButton();

    // 2. Expose the Mac's Fn/Globe-F shortcut through the selected fullscreen mode.
    native->viewMenuItem = [NSApp.mainMenu itemWithTitle:@"View"];
    if (!native->viewMenuItem) {
        native->viewMenuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
        native->viewMenuItem.submenu = [[NSMenu alloc] initWithTitle:@"View"];
        native->ownsViewMenu = true;
        [NSApp.mainMenu addItem:native->viewMenuItem];
    }
    NSMenu *viewMenu = native->viewMenuItem.submenu;
    native->fullscreenMenuItem = [[NSMenuItem alloc] initWithTitle:@"Enter Full Screen"
        action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    native->fullscreenMenuItem.keyEquivalentModifierMask = NSEventModifierFlagFunction;
    native->fullscreenMenuItem.target = native->target;
    [viewMenu addItem:native->fullscreenMenuItem];

    native->settingsMenuItem = [[NSMenuItem alloc] initWithTitle:@"Settings…" action:@selector(openSettings:) keyEquivalent:@","];
    native->settingsMenuItem.target = native->target;
    [NSApp.mainMenu.itemArray.firstObject.submenu insertItem:native->settingsMenuItem atIndex:1];

    // 3. Keep the title and inspector controls in the native titlebar, without a second app banner.
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    view.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    view.window.titlebarAppearsTransparent = YES;
    view.window.backgroundColor = [NSColor colorWithRed:0.09 green:0.115 blue:0.145 alpha:1];
    native->inspectorAccessory = [NSTitlebarAccessoryViewController new];
    native->inspectorAccessory.layoutAttribute = NSLayoutAttributeRight;
    NSView *accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 80, 28)];
    NSArray<NSString *> *symbols = @[@"captions.bubble", @"info.circle"];
    NSArray<NSString *> *labels = @[@"Toggle transcript", @"Show meaning"];
    for (NSUInteger index = 0; index < symbols.count; ++index) {
        NSButton *button = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:symbols[index] accessibilityDescription:labels[index]]
            target:native->target action:index == 0 ? @selector(showTranscript:) : @selector(showMeaning:)];
        button.frame = NSMakeRect(index * 36, 1, 32, 26);
        button.bordered = NO;
        button.toolTip = labels[index];
        [button setAccessibilityLabel:labels[index]];
        [accessoryView addSubview:button];
    }
    native->inspectorAccessory.view = accessoryView;
    [view.window addTitlebarAccessoryViewController:native->inspectorAccessory];

    // 4. Reconcile native exits initiated by macOS, and ignore toggles during animation.
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    for (NSNotificationName name in @[NSWindowWillEnterFullScreenNotification, NSWindowWillExitFullScreenNotification]) {
        [native->observers addObject:[center addObserverForName:name object:view.window queue:nil usingBlock:^(NSNotification *) {
            transitioning = true;
        }]];
    }
    for (NSNotificationName name in @[NSWindowDidEnterFullScreenNotification, NSWindowDidExitFullScreenNotification]) {
        [native->observers addObject:[center addObserverForName:name object:view.window queue:nil usingBlock:^(NSNotification *) {
            transitioning = false;
            fullscreenActive = (view.window.styleMask & NSWindowStyleMaskFullScreen) != 0;
            nativeSession = fullscreenActive;
            native->inspectorAccessory.view.hidden = fullscreenActive;
            native->fullscreenMenuItem.title = fullscreenActive ? @"Exit Full Screen" : @"Enter Full Screen";
            bindZoomButton();
            emit activeChanged();
        }]];
    }
}

int FullscreenController::safeTopInset() const {
    if (!window || !fullscreenActive || nativeSession) return 0;

    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    return int(view.window.screen.safeAreaInsets.top);
}

void FullscreenController::bindZoomButton() {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *nativeWindow = view.window;
    if (native->inspectorAccessory && (nativeWindow.styleMask & NSWindowStyleMaskTitled)) {
        if (![nativeWindow.titlebarAccessoryViewControllers containsObject:native->inspectorAccessory]) {
            [nativeWindow addTitlebarAccessoryViewController:native->inspectorAccessory];
        }
        native->inspectorAccessory.view.hidden = fullscreenActive;
    }
    nativeWindow.collectionBehavior = (nativeWindow.collectionBehavior
        & ~(NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorFullScreenAuxiliary)
        & ~NSWindowCollectionBehaviorFullScreenNone)
        | ((fullscreenActive ? nativeSession : preferredNativeMode)
            ? NSWindowCollectionBehaviorFullScreenPrimary : NSWindowCollectionBehaviorFullScreenNone);
    native->zoomButton = [view.window standardWindowButton:NSWindowZoomButton];
    if (native->zoomButton.target != native->target) {
        native->savedTarget = native->zoomButton.target;
        native->savedAction = native->zoomButton.action;
    }

    native->zoomButton.target = native->target;
    native->zoomButton.action = @selector(toggleFullScreen:);
}

void FullscreenController::toggle() {
    setActive(!fullscreenActive);
}

void FullscreenController::setActive(bool enabled) {
    if (!window || transitioning || enabled == fullscreenActive) {
        return;
    }

    // 1. Select the mode on entry; changing the preference never changes an active session.
    native->inspectorAccessory.view.hidden = enabled;
    if (enabled) nativeSession = preferredNativeMode;
    if (nativeSession) {
        transitioning = true;
        if (enabled) {
            savedVisibility = window->visibility();
            bindZoomButton();
            window->showFullScreen();
        } else if (savedVisibility == QWindow::Maximized) {
            window->showMaximized();
        } else {
            window->showNormal();
        }
        return;
    }

    // 2. Save the complete window state before covering its current screen.
    if (enabled) {
        savedGeometry = window->geometry();
        savedFlags = window->flags();
        savedVisibility = window->visibility();
        native->savedPresentation = NSApp.presentationOptions;
        NSApp.presentationOptions = NSApplicationPresentationAutoHideMenuBar
            | NSApplicationPresentationAutoHideDock;
        window->setFlag(Qt::FramelessWindowHint, true);
        QRect fullscreenGeometry = window->screen()->geometry();
        window->setGeometry(fullscreenGeometry);
    } else {

        // 3. Restore the original chrome, location, size, and system presentation.
        window->setFlags(savedFlags);
        window->setGeometry(savedGeometry);
        if (savedVisibility == QWindow::Maximized) {
            window->showMaximized();
        }
        NSApp.presentationOptions = native->savedPresentation;
    }

    // 4. Keep every application fullscreen control on this same transition path.
    fullscreenActive = enabled;
    // Qt resets the standard button action when window flags change.
    bindZoomButton();
    native->fullscreenMenuItem.title = enabled ? @"Exit Full Screen" : @"Enter Full Screen";
    window->raise();
    window->requestActivate();
    emit activeChanged();
}
