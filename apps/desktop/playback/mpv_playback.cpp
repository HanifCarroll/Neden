#include "mpv_playback.h"
#include "screenshot_image.h"
#include <QFutureWatcher>
#include <QPromise>
#include <QSaveFile>
#include <QThreadPool>
#include "media_path.h"
#include "video_geometry.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <QFileInfo>
#include <cmath>

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

    double secondaryDelay = 0;
    independentNativeSubtitleTiming =
        mpv_get_property(mpv.get(), "secondary-sub-delay", MPV_FORMAT_DOUBLE, &secondaryDelay) >= 0;
    seekTimer.setSingleShot(true);
    seekTimer.setInterval(80);
    connect(&seekTimer, &QTimer::timeout, this,
            [this] { command({"seek", QString::number(previewTarget, 'g', 17), "absolute+keyframes"}); });

    // 2. Deliver native events to Qt's owning thread, without a polling UI timer.
    mpv_observe_property(mpv.get(), positionProperty, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv.get(), durationProperty, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv.get(), pausedProperty, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv.get(), videoParametersProperty, "video-params", MPV_FORMAT_NODE);
    for (const char *name :
         {"track-list", "chapter-list", "audio-device-list", "audio-device", "seekable", "video-codec",
          "audio-codec-name", "hwdec-current", "video-params", "audio-params"})
        mpv_observe_property(mpv.get(), 10, name, MPV_FORMAT_NONE);
    mpv_observe_property(mpv.get(), 11, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv.get(), 12, "seeking", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv.get(), 13, "eof-reached", MPV_FORMAT_FLAG);
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
    const int result = mpv_command_async(mpv.get(), 0, pointers.data());
    if (result < 0)
        emit operationFailed(QString::fromUtf8(mpv_error_string(result)));
}

void MpvPlayback::open(const QString &path, bool loop) {

    // 1. Clear the previous geometry and retain only the latest requested media.
    seekTimer.stop();
    ++primarySubtitleGeneration;
    ++secondarySubtitleGeneration;
    subtitleRequests.clear();
    completed = false;
    playbackError.clear();
    updateState("loading");
    mediaTracks.clear();
    mediaChapters.clear();
    mediaInformation.clear();
    canSeek = false;
    emit metadataChanged();
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
    if (!std::isfinite(seconds))
        return;
    seekTimer.stop();
    ++primarySubtitleGeneration;
    ++secondarySubtitleGeneration;
    subtitleRequests.clear();
    completed = false;
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
                refreshMetadata();
                updateState("ready");
                emit loaded(loadedPath);
            }
        }
        if (event->event_id == MPV_EVENT_COMMAND_REPLY) {
            if (screenshotRequests.contains(event->reply_userdata)) {
                const auto path = screenshotRequests.take(event->reply_userdata);
                const auto *reply = static_cast<const mpv_event_command *>(event->data);
                if (event->error >= 0 && reply)
                    saveScreenshotResult(reply->result, path);
                else
                    screenshotBusy = false;
            }
            if (subtitleRequests.contains(event->reply_userdata)) {
                const auto request = subtitleRequests.take(event->reply_userdata);
                const auto currentGeneration =
                    request.secondary ? secondarySubtitleGeneration : primarySubtitleGeneration;
                if (event->error >= 0 && request.generation == currentGeneration) {
                    refreshMetadata();
                    // Match the requested file; another subtitle may have finished loading first.
                    for (auto entry = mediaTracks.crbegin(); entry != mediaTracks.crend(); ++entry) {
                        const auto track = entry->toMap();
                        if (track.value("type") == "sub" && track.value("external").toBool() &&
                            mediaPathIdentity(track.value("external-filename").toString()) == request.path) {
                            configure(request.secondary ? Option::SecondarySubtitle : Option::PrimarySubtitle,
                                      track.value("id"));
                            break;
                        }
                    }
                }
            }
            if (event->error < 0)
                emit operationFailed(QString::fromUtf8(mpv_error_string(event->error)));
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *ending = static_cast<mpv_event_end_file *>(event->data);
            if (ending->error < 0) {
                playbackError = QString("Playback failed: %1. Check the file or network connection.")
                                    .arg(QString::fromUtf8(mpv_error_string(ending->error)));
                updateState("error");
                emit failed();
            }
        }
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
        if (event->reply_userdata == 10) {
            refreshMetadata();
            continue;
        }
        if (!property->data)
            continue;
        if (event->reply_userdata >= 11 && event->reply_userdata <= 13) {
            const bool active = *static_cast<int *>(property->data);
            if (event->reply_userdata == 13 && active && !completed) {
                completed = true;
                updateState("finished");
                emit finished();
            } else if (!completed) {
                const bool buffering = this->property("paused-for-cache").toBool();
                const bool seeking = this->property("seeking").toBool();
                updateState(buffering ? "buffering" : seeking ? "seeking" : "ready");
            }
            continue;
        }

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

namespace {
QVariant nodeValue(const mpv_node &node, int depth = 0) {
    if (depth > 8)
        return {};
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG:
        return bool(node.u.flag);
    case MPV_FORMAT_INT64:
        return QVariant::fromValue<qlonglong>(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList values;
        if (node.u.list)
            for (int index = 0; index < qMin(node.u.list->num, 10000); ++index)
                values.append(nodeValue(node.u.list->values[index], depth + 1));
        return values;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap values;
        if (node.u.list)
            for (int index = 0; index < qMin(node.u.list->num, 256); ++index)
                values.insert(QString::fromUtf8(node.u.list->keys[index]),
                              nodeValue(node.u.list->values[index], depth + 1));
        return values;
    }
    default:
        return {};
    }
}
} // namespace
QVariant MpvPlayback::property(const char *name) const {
    mpv_node node{};
    if (mpv_get_property(mpv.get(), name, MPV_FORMAT_NODE, &node) < 0)
        return {};
    const auto result = nodeValue(node);
    mpv_free_node_contents(&node);
    return result;
}
void MpvPlayback::setPropertyValue(const char *name, const QVariant &value) {
    const auto bytes = value.toString().toUtf8();
    const int result = mpv_set_property_string(mpv.get(), name, bytes.constData());
    if (result < 0)
        emit operationFailed(QString("Cannot change %1: %2")
                                 .arg(QString::fromUtf8(name), QString::fromUtf8(mpv_error_string(result))));
}
void MpvPlayback::refreshMetadata() {
    const auto previousDevices = outputDevices;
    mediaTracks = property("track-list").toList();
    for (auto &entry : mediaTracks) {
        auto track = entry.toMap();
        QStringList label;
        label << QString::number(track.value("id").toLongLong());
        for (const auto &field : {"lang", "title", "codec"}) {
            const auto value = track.value(field).toString();
            if (!value.isEmpty())
                label << value;
        }
        if (track.value("default").toBool())
            label << "Default";
        if (track.value("forced").toBool())
            label << "Forced";
        track.insert("label", label.join(" · "));
        entry = track;
    }
    mediaChapters = property("chapter-list").toList();
    outputDevices = property("audio-device-list").toList();
    canSeek = property("seekable").toBool();
    mediaInformation = {{"Video codec", property("video-codec")},
                        {"Audio codec", property("audio-codec-name")},
                        {"Hardware decoder", property("hwdec-current")},
                        {"Video", property("video-params")},
                        {"Audio", property("audio-params")}};
    if (!previousDevices.isEmpty() && outputDevices.size() < previousDevices.size())
        emit outputDeviceRemoved();
    emit metadataChanged();
}
void MpvPlayback::updateState(const QString &state) {
    if (playbackState == state)
        return;
    playbackState = state;
    emit stateChanged();
}
void MpvPlayback::previewSeek(double seconds) {
    if (!std::isfinite(seconds) || !canSeek)
        return;
    previewTarget = qBound(0.0, seconds, durationSeconds);
    if (!seekTimer.isActive())
        seekTimer.start();
}
void MpvPlayback::configure(Option option, const QVariant &value) {
    switch (option) {
    case Option::AudioTrack:
        setPropertyValue("aid", value);
        break;
    case Option::PrimarySubtitle:
        ++primarySubtitleGeneration;
        setPropertyValue("sid", value);
        break;
    case Option::SecondarySubtitle:
        ++secondarySubtitleGeneration;
        setPropertyValue("secondary-sid", value);
        break;
    case Option::AudioDevice:
        setPropertyValue("audio-device", value);
        break;
    case Option::AudioDelay:
        setPropertyValue("audio-delay", value);
        break;
    case Option::PrimarySubtitleDelay:
        setPropertyValue("sub-delay", value);
        break;
    case Option::SecondarySubtitleDelay:
        if (independentNativeSubtitleTiming)
            setPropertyValue("secondary-sub-delay", value);
        break;
    case Option::SubtitleEncoding:
        setPropertyValue("sub-codepage", value);
        break;
    case Option::Rotation:
        setPropertyValue("video-rotate", value);
        break;
    case Option::AspectRatio:
        setPropertyValue("video-aspect-override", value);
        break;
    case Option::Fill:
        setPropertyValue("panscan", value.toBool() ? 1 : 0);
        break;
    case Option::Deinterlace:
        setPropertyValue("deinterlace", value.toBool() ? "yes" : "no");
        break;
    case Option::NightMode:
        command(value.toBool() ? QStringList{"af", "add",
                                             "@neden-night:lavfi=[acompressor=threshold=0.125:ratio=4:attack="
                                             "20:release=250:makeup=2]"}
                               : QStringList{"af", "remove", "@neden-night"});
        break;
    case Option::LoopStart:
        setPropertyValue("ab-loop-a", value);
        break;
    case Option::LoopEnd:
        setPropertyValue("ab-loop-b", value);
        break;
    }
}
void MpvPlayback::execute(Command action) {
    switch (action) {
    case Command::FrameForward:
        command({"frame-step"});
        break;
    case Command::FrameBackward:
        command({"frame-back-step"});
        break;
    case Command::Stop:
        seekTimer.stop();
        command({"stop"});
        updateState("idle");
        break;
    }
}
void MpvPlayback::addSubtitle(const QString &path, bool secondary) {
    const auto encoded = path.toUtf8();
    const char *arguments[] = {"sub-add", encoded.constData(), "auto", nullptr};
    const auto request = ++nextRequest;
    subtitleRequests.insert(request,
                            {mediaPathIdentity(path), secondary,
                             secondary ? ++secondarySubtitleGeneration : ++primarySubtitleGeneration});
    const int result = mpv_command_async(mpv.get(), request, arguments);
    if (result < 0) {
        subtitleRequests.remove(request);
        emit operationFailed("This subtitle track could not be loaded.");
    }
}
void MpvPlayback::screenshot(const QString &path, bool subtitles) {
    if (screenshotBusy) {
        emit operationFailed("A screenshot is already being saved.");
        return;
    }
    screenshotBusy = true;
    const char *arguments[] = {"screenshot-raw", subtitles ? "subtitles" : "video", nullptr};
    const auto request = ++nextRequest;
    screenshotRequests.insert(request, path);
    if (mpv_command_async(mpv.get(), request, arguments) < 0) {
        screenshotRequests.remove(request);
        screenshotBusy = false;
        emit operationFailed("The screenshot could not be saved.");
    }
}

void MpvPlayback::saveScreenshotResult(const mpv_node &result, const QString &path) {
    const QImage image = screenshotImage(result);
    if (image.isNull()) {
        screenshotBusy = false;
        emit operationFailed("The current video frame could not be captured.");
        return;
    }

    // Encoding and disk I/O run away from the playback/UI thread.
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path] {
        screenshotBusy = false;
        if (watcher->result())
            emit screenshotSaved(path);
        else
            emit operationFailed("The screenshot could not be saved. Check the destination folder.");
        watcher->deleteLater();
    });
    QPromise<bool> completion;
    watcher->setFuture(completion.future());
    QThreadPool::globalInstance()->start([image, path, completion = std::move(completion)]() mutable {
        completion.start();
        QSaveFile destination(path);
        const bool saved =
            destination.open(QIODevice::WriteOnly) && image.save(&destination, "PNG") && destination.commit();
        completion.addResult(saved);
        completion.finish();
    });
}
