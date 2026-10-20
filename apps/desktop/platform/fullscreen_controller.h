#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>
#include <memory>

class FullscreenController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fullscreen control is supplied by the application")
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int safeTopInset READ safeTopInset NOTIFY activeChanged)
    Q_PROPERTY(bool nativeMode READ nativeMode WRITE setNativeMode NOTIFY nativeModeChanged)
    Q_PROPERTY(bool supportsNativeMode READ supportsNativeMode CONSTANT)
public:
    explicit FullscreenController(QObject *parent = nullptr);
    ~FullscreenController() override;
    void attach(QQuickWindow *window);
    int safeTopInset() const;
    bool active() const { return fullscreenActive; }
    bool nativeMode() const { return preferredNativeMode; }
    bool supportsNativeMode() const {
#ifdef Q_OS_MACOS
        return true;
#else
        return false;
#endif
    }
    void setNativeMode(bool enabled);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setActive(bool enabled);
signals:
    void settingsRequested();
    void activeChanged();
    void nativeModeChanged();
private:
    void bindZoomButton();
    struct NativeState;
    std::unique_ptr<NativeState> native;
    QPointer<QQuickWindow> window;
    QRect savedGeometry;
    Qt::WindowFlags savedFlags;
    QWindow::Visibility savedVisibility = QWindow::Windowed;
    bool fullscreenActive = false;
    bool preferredNativeMode = false;
    bool nativeSession = false;
    bool transitioning = false;
};
