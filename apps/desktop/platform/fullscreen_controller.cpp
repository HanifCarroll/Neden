#include "fullscreen_controller.h"

struct FullscreenController::NativeState {};
FullscreenController::FullscreenController(QObject *parent) : QObject(parent), native(std::make_unique<NativeState>()) {}
FullscreenController::~FullscreenController() = default;

void FullscreenController::attach(QQuickWindow *target) {
    window = target;
    connect(target, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
        const bool active = visibility == QWindow::FullScreen;
        if (active != fullscreenActive) {
            fullscreenActive = active;
            emit activeChanged();
        }
    });
}
int FullscreenController::safeTopInset() const { return 0; }
void FullscreenController::bindZoomButton() {}
void FullscreenController::setNativeMode(bool) {
    // Other platforms use the window manager's fullscreen mechanism without macOS Spaces.
}
void FullscreenController::toggle() { setActive(!fullscreenActive); }
void FullscreenController::setActive(bool enabled) {
    if (!window || enabled == fullscreenActive) return;

    if (enabled) {
        savedVisibility = window->visibility();
        window->showFullScreen();
    } else if (savedVisibility == QWindow::Maximized) {
        window->showMaximized();
    } else {
        window->showNormal();
    }
}
