#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

class PlaybackEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Playback is supplied by the application")
public:
    using QObject::QObject;
    double position() const { return positionSeconds; }
    virtual double snapshotPosition() const { return positionSeconds; }
    double duration() const { return durationSeconds; }
    bool paused() const { return isPaused; }
    virtual void open(const QString &path, bool loop) = 0;
    virtual void seek(double seconds) = 0;
    virtual void setPaused(bool value) = 0;
    virtual void rate(double value) = 0;
    virtual void volume(double value) = 0;
signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void loaded(QString path);
    void failed();

protected:
    double positionSeconds = 0;
    double durationSeconds = 0;
    bool isPaused = true;
};
