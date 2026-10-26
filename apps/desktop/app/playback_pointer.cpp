#include "playback_pointer.h"
#include <QCursor>
#include <QEnterEvent>
#include <QGuiApplication>
#include <QMouseEvent>

PlaybackPointer::PlaybackPointer(QObject *parent) : QObject(parent) {
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow *focused) {
        if (focused != playerWindow) {
            pointerInside = false;
            reveal();
        }
    });
    connect(qGuiApp, &QGuiApplication::aboutToQuit, this, &PlaybackPointer::restoreCursor);
}

PlaybackPointer::~PlaybackPointer() {
    restoreCursor();
}

void PlaybackPointer::setWindow(QQuickWindow *window) {
    if (playerWindow == window)
        return;
    restoreCursor();
    if (playerWindow) {
        playerWindow->removeEventFilter(this);
        disconnect(playerWindow, nullptr, this, nullptr);
    }
    playerWindow = window;
    pointerInside = false;
    if (playerWindow) {
        playerWindow->installEventFilter(this);
        connect(playerWindow, &QObject::destroyed, this, &PlaybackPointer::restoreCursor);
    }
    emit windowChanged();
}

void PlaybackPointer::setVideoArea(QQuickItem *item) {
    if (videoItem == item)
        return;
    restoreCursor();
    videoItem = item;
    emit videoAreaChanged();
}

void PlaybackPointer::setHideRequested(bool requested) {
    if (shouldHide == requested)
        return;
    shouldHide = requested;
    updateCursor();
    emit hideRequestedChanged();
}

void PlaybackPointer::restoreCursor() {
    if (!ownsBlankCursor)
        return;
    QGuiApplication::restoreOverrideCursor();
    ownsBlankCursor = false;
    emit hiddenChanged();
}

void PlaybackPointer::reveal() {
    restoreCursor();
    emit activity();
}

void PlaybackPointer::updateCursor() {
    const bool overVideo = playerWindow && videoItem && pointerInside && videoItem->isVisible()
        && videoItem->contains(videoItem->mapFromScene(lastPosition));
    if (!shouldHide || !overVideo || !playerWindow->isActive() || !playerWindow->isVisible()) {
        restoreCursor();
        return;
    }
    // A single balanced override prevents subtitle/button cursor shapes from
    // fighting the idle state. Never cover another operation's override.
    if (!ownsBlankCursor && !QGuiApplication::overrideCursor()) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
        ownsBlankCursor = true;
        emit hiddenChanged();
    }
}

bool PlaybackPointer::eventFilter(QObject *object, QEvent *event) {
    if (object != playerWindow)
        return false;
    switch (event->type()) {
    case QEvent::MouseMove: {
        const auto position = static_cast<QMouseEvent *>(event)->position();
        if (!pointerInside || position != lastPosition) {
            lastPosition = position;
            pointerInside = true;
            reveal();
        }
        break;
    }
    case QEvent::Enter:
        lastPosition = static_cast<QEnterEvent *>(event)->position();
        pointerInside = true;
        reveal();
        break;
    case QEvent::Leave:
    case QEvent::WindowDeactivate:
    case QEvent::Hide:
    case QEvent::Close:
        pointerInside = false;
        reveal();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::WindowActivate:
        reveal();
        break;
    default:
        break;
    }
    // Observing activity must not consume clicks, selection, or scrolling.
    return false;
}
