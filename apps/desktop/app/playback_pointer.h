#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

// Observes window input before child controls consume it and owns the idle cursor.
class PlaybackPointer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QQuickItem *videoArea READ videoArea WRITE setVideoArea NOTIFY videoAreaChanged)
    Q_PROPERTY(bool hideRequested READ hideRequested WRITE setHideRequested NOTIFY hideRequestedChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY hiddenChanged)
public:
    explicit PlaybackPointer(QObject *parent = nullptr);
    ~PlaybackPointer() override;
    QQuickWindow *window() const { return playerWindow; }
    void setWindow(QQuickWindow *window);
    QQuickItem *videoArea() const { return videoItem; }
    void setVideoArea(QQuickItem *item);
    bool hideRequested() const { return shouldHide; }
    void setHideRequested(bool requested);
    bool hidden() const { return ownsBlankCursor; }
signals:
    void windowChanged();
    void videoAreaChanged();
    void hideRequestedChanged();
    void hiddenChanged();
    void activity();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void reveal();
    void restoreCursor();
    void updateCursor();
    QPointer<QQuickWindow> playerWindow;
    QPointer<QQuickItem> videoItem;
    QPointF lastPosition;
    bool pointerInside = false;
    bool shouldHide = false;
    bool ownsBlankCursor = false;
};
