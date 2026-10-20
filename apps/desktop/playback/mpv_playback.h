#pragma once
#include "playback_engine.h"
#include "video_render_lifetime.h"
#include <QJsonObject>
#include <QSize>
#include <mpv/client.h>

class MpvPlayback : public PlaybackEngine {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Playback is supplied by the application")
    Q_PROPERTY(QSize sourceOutputSize READ sourceOutputSize NOTIFY videoGeometryChanged)
public:
    explicit MpvPlayback(QObject *parent = nullptr);
    ~MpvPlayback() override;
    mpv_handle *handle() const { return mpv; }
    void open(const QString &path, bool loop) override;
    void seek(double seconds) override;
    double snapshotPosition() const override;
    void setPaused(bool value) override;
    void rate(double value) override;
    void volume(double value) override;
    QSize sourceOutputSize() const { return sourceSize; }
    bool videoOutputRequested() const { return outputRequested; }
    VideoRenderLifetime::Token attachVideoRenderer();
    void detachVideoRenderer(VideoRenderLifetime::Token token);
signals:
    void videoOutputNeeded();
    void videoGeometryChanged();
    void videoGeometryInvalid();
private:
    void command(const QStringList &arguments);
    void drainEvents();
    void loadPendingMedia();
    mpv_handle *mpv = nullptr;
    QSize sourceSize;
    QJsonObject sourceParameters;
    VideoRenderLifetime renderLifetime;
    QString requestedPath;
    QString pendingPath;
    bool pendingLoop = false;
    bool outputRequested = false;
};
