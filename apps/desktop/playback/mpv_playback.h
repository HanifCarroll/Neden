#pragma once
#include "playback_engine.h"
#include "video_render_lifetime.h"
#include <QJsonObject>
#include <QSize>
#include <mpv/client.h>
#include <memory>
#include <QTimer>

class MpvPlayback : public PlaybackEngine {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Playback is supplied by the application")
    Q_PROPERTY(QSize sourceOutputSize READ sourceOutputSize NOTIFY videoGeometryChanged)
public:
    explicit MpvPlayback(QObject *parent = nullptr);
    ~MpvPlayback() override;
    mpv_handle *handle() const { return mpv.get(); }
    void open(const QString &path, bool loop) override;
    void seek(double seconds) override;
    double snapshotPosition() const override;
    void setPaused(bool value) override;
    void rate(double value) override;
    void volume(double value) override;
    void configure(Option option, const QVariant &value) override;
    void execute(Command command) override;
    void previewSeek(double seconds) override;
    void addSubtitle(const QString &path, bool secondary) override;
    void screenshot(const QString &path, bool subtitles) override;
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
    void refreshMetadata();
    void updateState(const QString &state);
    QVariant property(const char *name) const;
    void setPropertyValue(const char *name, const QVariant &value);
    QTimer seekTimer;
    double previewTarget = 0;
    bool completed = false;
    quint64 nextRequest = 100;
    QHash<quint64, QString> screenshotRequests;
    struct SubtitleRequest {
        QString path;
        bool secondary;
        quint64 generation;
    };
    QHash<quint64, SubtitleRequest> subtitleRequests;
    quint64 primarySubtitleGeneration = 0;
    quint64 secondarySubtitleGeneration = 0;

    void loadPendingMedia();
    struct HandleDeleter {
        void operator()(mpv_handle *handle) const noexcept { mpv_terminate_destroy(handle); }
    };
    std::unique_ptr<mpv_handle, HandleDeleter> mpv;
    QSize sourceSize;
    QJsonObject sourceParameters;
    VideoRenderLifetime renderLifetime;
    QString requestedPath;
    QString pendingPath;
    bool pendingLoop = false;
    bool outputRequested = false;
};
