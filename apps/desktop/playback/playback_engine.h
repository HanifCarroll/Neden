#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
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

    // Optional operations default to no-op for lightweight test engines.
    enum class Option { AudioTrack, PrimarySubtitle, SecondarySubtitle, AudioDevice, AudioDelay,
                        PrimarySubtitleDelay, SecondarySubtitleDelay, SubtitleEncoding,
                        Rotation, AspectRatio, Fill, Deinterlace, NightMode, LoopStart, LoopEnd };
    enum class Command { FrameForward, FrameBackward, Stop };
    virtual void configure(Option, const QVariant &) {}
    virtual void execute(Command) {}
    virtual void previewSeek(double seconds) { seek(seconds); }
    virtual void addSubtitle(const QString &, bool) {}
    virtual void screenshot(const QString &, bool) {}
    QVariantList tracks() const { return mediaTracks; }
    QVariantList chapters() const { return mediaChapters; }
    QVariantList audioDevices() const { return outputDevices; }
    QVariantMap information() const { return mediaInformation; }
    QString state() const { return playbackState; }
    QString error() const { return playbackError; }
    bool seekable() const { return canSeek; }

signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void loaded(QString path);
    void failed();
    void metadataChanged();
    void stateChanged();
    void finished();
    void outputDeviceRemoved();
    void operationFailed(QString message);
    void screenshotSaved(QString path);


protected:
    QVariantList mediaTracks, mediaChapters, outputDevices;
    QVariantMap mediaInformation;
    QString playbackState = "idle", playbackError;
    bool canSeek = false;
    double positionSeconds = 0;
    double durationSeconds = 0;
    bool isPaused = true;
};
