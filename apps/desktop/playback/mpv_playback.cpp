#include "mpv_playback.h"
#include "media_path.h"
#include "video_geometry.h"
#include <stdexcept>

static QJsonObject videoParameterMetadata(const mpv_node *node) {

    // 1. Bound the native map before inspecting any parameter values.
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list) return {};

    const auto *map = node->u.list;
    if (map->num < 0 || map->num > 128 || !map->keys || !map->values) return {};

    // 2. Retain only geometry numbers, excluding text or media identifiers.
    QJsonObject result;
    for (int index = 0; index < map->num; ++index) {
        if (!map->keys[index]) continue;

        const QString key = QString::fromUtf8(map->keys[index]);
        const auto &value = map->values[index];
        if (QStringList{"w", "h", "dw", "dh", "rotate"}.contains(key)
            && value.format == MPV_FORMAT_INT64) {
            result.insert(key, qint64(value.u.int64));
        }
    }
    return result;
}

MpvPlayback::MpvPlayback(QObject *parent) : PlaybackEngine(parent) {

    // 1. Create the reviewed native player with bounded playback buffers.
    mpv = mpv_create();
    if (!mpv) throw std::runtime_error("Cannot create playback engine.");

    for (const auto &option : QList<QPair<const char *, const char *>>{
        {"config", "no"}, {"vo", "libmpv"}, {"hwdec", "auto-safe"}, {"pause", "yes"},
        {"keep-open", "yes"}, {"volume", "60"}, {"sid", "no"},
        // Paired with BLOCK_FOR_TARGET_TIME=0 in VideoRenderer.
        {"video-timing-offset", "0"},
        {"demuxer-max-bytes", "33554432"}, {"demuxer-max-back-bytes", "8388608"}}) {
        if (mpv_set_option_string(mpv, option.first, option.second) < 0) {
            mpv_terminate_destroy(mpv);
            throw std::runtime_error("Unsupported playback option.");
        }
    }
    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        throw std::runtime_error("Cannot initialize playback.");
    }

    // 2. Deliver native events to Qt's owning thread, without a polling UI timer.
    mpv_observe_property(mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 4, "video-params", MPV_FORMAT_NODE);
    mpv_set_wakeup_callback(mpv, [](void *context) {
        auto *playback = static_cast<MpvPlayback *>(context);
        QMetaObject::invokeMethod(playback, [playback] { playback->drainEvents(); }, Qt::QueuedConnection);
    }, this);
}

MpvPlayback::~MpvPlayback() {
    mpv_set_wakeup_callback(mpv, nullptr, nullptr);
    mpv_terminate_destroy(mpv);
}

void MpvPlayback::command(const QStringList &arguments) {
    QList<QByteArray> bytes;
    for (const auto &argument : arguments) bytes.append(argument.toUtf8());
    QVector<const char *> pointers;
    for (const auto &argument : bytes) pointers.append(argument.constData());
    pointers.append(nullptr);
    if (mpv_command(mpv, pointers.data()) < 0) emit failed();
}

void MpvPlayback::open(const QString &path, bool loop) {

    // 1. Clear the previous geometry and retain only the latest requested media.
    sourceSize = {};
    sourceParameters = {};
    emit videoGeometryChanged();
    positionSeconds = 0;
    durationSeconds = 0;
    emit positionChanged();
    emit durationChanged();
    setPaused(true);
    requestedPath = mediaPathIdentity(path);
    pendingPath = requestedPath;
    pendingLoop = loop;
    outputRequested = true;

    // 2. mpv requires a live render context before initializing video output.
    if (renderLifetime.available()) {
        loadPendingMedia();
    } else {
        emit videoOutputNeeded();
    }
}

VideoRenderLifetime::Token MpvPlayback::attachVideoRenderer() {
    const auto token = renderLifetime.activate();
    QMetaObject::invokeMethod(this, [this, token] {
        if (renderLifetime.owns(token)) loadPendingMedia();
    }, Qt::QueuedConnection);
    return token;
}

void MpvPlayback::detachVideoRenderer(VideoRenderLifetime::Token token) {
    renderLifetime.release(token);
}

void MpvPlayback::loadPendingMedia() {
    if (pendingPath.isEmpty() || !renderLifetime.available()) return;

    const auto path = pendingPath;
    pendingPath.clear();
    mpv_set_property_string(mpv, "loop-file", pendingLoop ? "inf" : "no");
    command({"loadfile", path});
}
void MpvPlayback::seek(double seconds) { command({"seek", QString::number(seconds, 'g', 17), "absolute+exact"}); }
double MpvPlayback::snapshotPosition() const {
    double seconds = positionSeconds;
    mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &seconds);
    return seconds;
}
void MpvPlayback::setPaused(bool value) {
    int flag = value;
    if (mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &flag) < 0) {
        emit failed();
        return;
    }

    if (isPaused != value) {
        isPaused = value;
        emit pausedChanged();
    }
}
void MpvPlayback::rate(double value) { mpv_set_property(mpv, "speed", MPV_FORMAT_DOUBLE, &value); }
void MpvPlayback::volume(double value) { mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &value); }

void MpvPlayback::drainEvents() {
    while (auto *event = mpv_wait_event(mpv, 0)) {
        if (event->event_id == MPV_EVENT_NONE) break;

        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            char *path = mpv_get_property_string(mpv, "path");
            const auto loadedPath = mediaPathIdentity(QString::fromUtf8(path ? path : ""));
            mpv_free(path);
            if (loadedPath == requestedPath) {
                mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &durationSeconds);
                emit durationChanged();
                emit loaded(loadedPath);
            }
        }
        if (event->event_id == MPV_EVENT_END_FILE && static_cast<mpv_event_end_file *>(event->data)->error < 0) emit failed();
        if (event->event_id != MPV_EVENT_PROPERTY_CHANGE) continue;

        const auto *property = static_cast<mpv_event_property *>(event->data);
        if (event->reply_userdata == 4) {
            const auto metadata = property->format == MPV_FORMAT_NODE
                ? videoParameterMetadata(static_cast<const mpv_node *>(property->data)) : QJsonObject{};
            if (metadata == sourceParameters) continue;

            sourceParameters = metadata;
            sourceSize = {};
            if (metadata.contains("rotate") && metadata["w"].toInt() > 0 && metadata["h"].toInt() > 0) {
                try {
                    sourceSize = rotatedVideoSize({metadata["dw"].toInt(), metadata["dh"].toInt()},
                                                  metadata["rotate"].toInt(-1));
                } catch (const std::invalid_argument &) {
                    emit videoGeometryInvalid();
                }
            }
            emit videoGeometryChanged();
            continue;
        }
        if (!property->data) continue;

        if (event->reply_userdata == 1) {
            const double value = *static_cast<double *>(property->data);
            if (positionSeconds != value) { positionSeconds = value; emit positionChanged(); }
        } else if (event->reply_userdata == 2) {
            const double value = *static_cast<double *>(property->data);
            if (durationSeconds != value) { durationSeconds = value; emit durationChanged(); }
        } else if (event->reply_userdata == 3) {
            const bool value = *static_cast<int *>(property->data);
            if (isPaused != value) { isPaused = value; emit pausedChanged(); }
        }
    }
}
