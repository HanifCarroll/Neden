#import <AppKit/AppKit.h>

bool nativeMenusAreAvailable() {
    for (NSString *title in @[@"File", @"Playback", @"Subtitles", @"Study", @"View"]) {
        NSMenuItem *item = [NSApp.mainMenu itemWithTitle:title];
        if (!item || item.submenu.numberOfItems == 0) return false;
    }
    return true;
}
