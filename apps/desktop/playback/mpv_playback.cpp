#include "mpv_playback.h"
#include "media_path.h"
#include "video_geometry.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {
constexpr auto positionProperty = std::uint64_t{1};
constexpr auto durationProperty = std::uint64_t{2};
constexpr auto pausedProperty = std::uint64_t{3};
constexpr auto videoParametersProperty = std::uint64_t{4};
constexpr std::array playbackOptions{
    std::pair{"config", "no"}, std::pair{"vo", "libmpv"}, std::pair{"hwdec", "auto-safe"},
    std::pair{"pause", "yes"}, std::pair{"keep-open", "yes"}, std::pair{"volume", "60"},
    std::pair{"sid", "no"},
    // Paired with BLOCK_FOR_TARGET_TIME=0 in VideoRenderer.
    std::pair{"video-timing-offset", "0"}, std::pair{"demuxer-max-bytes", "33554432"},
    std::pair{"demuxer-max-back-bytes", "8388608"}};
} // namespace

static QJsonObject videoParameterMetadata(const mpv_node *node) {

    // 1. Bound the native map before inspecting any parameter values.
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
        return {};

    const auto *map = node->u.list;
    if (map->num < 0 || map->num > 128 || !map->keys || !map->values)
        return {};

    // 2. Retain only geometry numbers, excluding text or media identifiers.
    QJsonObject result;
    for (int index = 0; index < map->num; ++index) {
        if (!map->keys[index])
            continue;

        const QString key = QString::fromUtf8(map->keys[index]);
        const auto &value = map->values[index];
        if (QStringList{"w", "h", "dw", "dh", "rotate"}.contains(key) && value.format == MPV_FORMAT_INT64) {
            result.insert(key, static_cast<qint64>(value.u.int64));
        }
    }
    return result;
}

MpvPlayback::MpvPlayback(QObject *parent) : PlaybackEngine(parent), mpv(mpv_create()) {

    // 1. Create the reviewed native player with bounded playback buffers.
    if (!mpv)
        throw std::runtime_error("Cannot create playback engine.");

    for (const auto &[name, value] : playbackOptions) {
        if (mpv_set_option_string(mpv.get(), name, value) < 0) {
            throw std::runtime_error("Unsupported playback option.");
        }
    }
    if (mpv_initialize(mpv.get()) < 0) {
        throw std::runtime_error("Cannot initialize playback.");
    }

    // 2. Deliver native events to Qt's owning thread, without a polling UI timer.
    mpv_observe_property(mpv.get(), positionProperty, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv.get(), durationProperty, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv.get(), pausedProperty, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv.get(), videoParametersProperty, "video-params", MPV_FORMAT_NODE);
    mpv_set_wakeup_callback(
        mpv.get(),
        [](void *context) {
            auto *playback = static_cast<MpvPlayback *>(context);
            QMetaObject::invokeMethod(
                playback, [playback] { playback->drainEvents(); }, Qt::QueuedConnection);
        },
        this);
}

MpvPlayback::~MpvPlayback() {
    mpv_set_wakeup_callback(mpv.get(), nullptr, nullptr);
}

void MpvPlayback::command(const QStringList &arguments) {
    QList<QByteArray> bytes;
    for (const auto &argument : arguments)
        bytes.append(argument.toUtf8());
    QVector<const char *> pointers;
    for (const auto &argument : bytes)
        pointers.append(argument.constData());
    pointers.append(nullptr);
    if (mpv_command(mpv.get(), pointers.data()) < 0)
        emit failed();
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
    QMetaObject::invokeMethod(
        this,
        [this, token] {
            if (renderLifetime.owns(token))
                loadPendingMedia();
        },
        Qt::QueuedConnection);
    return token;
}

void MpvPlayback::detachVideoRenderer(VideoRenderLifetime::Token token) {
    renderLifetime.release(token);
}

void MpvPlayback::loadPendingMedia() {
    if (pendingPath.isEmpty() || !renderLifetime.available())
        return;

    const auto path = pendingPath;
    pendingPath.clear();
    mpv_set_property_string(mpv.get(), "loop-file", pendingLoop ? "inf" : "no");
    command({"loadfile", path});
}
void MpvPlayback::seek(double seconds) {
    command({"seek", QString::number(seconds, 'g', 17), "absolute+exact"});
}
double MpvPlayback::snapshotPosition() const {
    double seconds = positionSeconds;
    mpv_get_property(mpv.get(), "time-pos", MPV_FORMAT_DOUBLE, &seconds);
    return seconds;
}
void MpvPlayback::setPaused(bool value) {
    int flag = value;
    if (mpv_set_property(mpv.get(), "pause", MPV_FORMAT_FLAG, &flag) < 0) {
        emit failed();
        return;
    }

    if (isPaused != value) {
        isPaused = value;
        emit pausedChanged();
    }
}
void MpvPlayback::rate(double value) {
    mpv_set_property(mpv.get(), "speed", MPV_FORMAT_DOUBLE, &value);
}
void MpvPlayback::volume(double value) {
    mpv_set_property(mpv.get(), "volume", MPV_FORMAT_DOUBLE, &value);
}

void MpvPlayback::drainEvents() {
    while (auto *event = mpv_wait_event(mpv.get(), 0)) {
        if (event->event_id == MPV_EVENT_NONE)
            break;

        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            const std::unique_ptr<char, decltype(&mpv_free)> path(mpv_get_property_string(mpv.get(), "path"),
                                                                  &mpv_free);
            const auto loadedPath = mediaPathIdentity(QString::fromUtf8(path ? path.get() : ""));
            if (loadedPath == requestedPath) {
                mpv_get_property(mpv.get(), "duration", MPV_FORMAT_DOUBLE, &durationSeconds);
                emit durationChanged();
                emit loaded(loadedPath);
            }
        }
        if (event->event_id == MPV_EVENT_END_FILE &&
            static_cast<mpv_event_end_file *>(event->data)->error < 0)
            emit failed();
        if (event->event_id != MPV_EVENT_PROPERTY_CHANGE)
            continue;

        const auto *property = static_cast<mpv_event_property *>(event->data);
        if (event->reply_userdata == videoParametersProperty) {
            const auto metadata = property->format == MPV_FORMAT_NODE
                                      ? videoParameterMetadata(static_cast<const mpv_node *>(property->data))
                                      : QJsonObject{};
            if (metadata == sourceParameters)
                continue;

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
        if (!property->data)
            continue;

        if (event->reply_userdata == positionProperty) {
            const double value = *static_cast<double *>(property->data);
            if (positionSeconds != value) {
                positionSeconds = value;
                emit positionChanged();
            }
        } else if (event->reply_userdata == durationProperty) {
            const double value = *static_cast<double *>(property->data);
            if (durationSeconds != value) {
                durationSeconds = value;
                emit durationChanged();
            }
        } else if (event->reply_userdata == pausedProperty) {
            const bool value = *static_cast<int *>(property->data);
            if (isPaused != value) {
                isPaused = value;
                emit pausedChanged();
            }
        }
    }
}
