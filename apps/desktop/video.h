#pragma once
#include <QQuickFramebufferObject>
#include "mpv_playback.h"

class Video : public QQuickFramebufferObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(VideoSurface)
    Q_PROPERTY(MpvPlayback *playback READ playback WRITE setPlayback NOTIFY playbackChanged REQUIRED)
public:
    explicit Video(QQuickItem *parent = nullptr) : QQuickFramebufferObject(parent) {}
    Renderer *createRenderer() const override;
    MpvPlayback *playback() const { return engine; }
    void setPlayback(MpvPlayback *value) {
        if (engine == value) return;

        if (engine) disconnect(engine, nullptr, this, nullptr);
        engine = value;
        if (engine) {
            connect(engine, &MpvPlayback::videoGeometryChanged, this, &Video::update);
            connect(engine, &MpvPlayback::videoOutputNeeded, this, &Video::update);
        }
        emit playbackChanged();
    }
signals:
    void playbackChanged();
private:
    MpvPlayback *engine = nullptr;
};
