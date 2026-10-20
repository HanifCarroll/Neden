#include "thumbnail_service.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>
#include <mpv/client.h>
#include <mpv/render.h>
#include <memory>
#include <cmath>
#include <climits>

namespace {
struct ClientDeleter {
    void operator()(mpv_handle *client) const { mpv_terminate_destroy(client); }
};
struct RendererDeleter {
    void operator()(mpv_render_context *renderer) const { mpv_render_context_free(renderer); }
};
using Client = std::unique_ptr<mpv_handle, ClientDeleter>;
using Renderer = std::unique_ptr<mpv_render_context, RendererDeleter>;
QImage renderThumbnail(mpv_render_context *renderer) {
    QImage image(320, 180, QImage::Format_RGBX8888);
    int dimensions[]{image.width(), image.height()};
    size_t stride = static_cast<size_t>(image.bytesPerLine());
    int blocking = 0;
    mpv_render_param parameters[] = {{MPV_RENDER_PARAM_SW_SIZE, dimensions},
                                     {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
                                     {MPV_RENDER_PARAM_SW_STRIDE, &stride},
                                     {MPV_RENDER_PARAM_SW_POINTER, image.bits()},
                                     {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blocking},
                                     {MPV_RENDER_PARAM_INVALID, nullptr}};
    return mpv_render_context_render(renderer, parameters) < 0 ? QImage() : image;
}
} // namespace
ThumbnailWorker::~ThumbnailWorker() {
    {
        QMutexLocker lock(&mutex);
        stopping = true;
        pending.wakeOne();
    }
    wait();
}
QString ThumbnailRequest::cacheKey() const {
    return fileIdentity + ":" + QString::number(second) + (precise ? ":exact" : ":keyframe");
}
void ThumbnailWorker::request(const ThumbnailRequest &request) {
    QMutexLocker lock(&mutex);
    pendingRequest = request;
    requested = true;
    if (!isRunning())
        start();
    pending.wakeOne();
}
void ThumbnailWorker::cancel() {
    QMutexLocker lock(&mutex);
    ++cancellation;
    requested = false;
    pending.wakeOne();
}
void ThumbnailWorker::run() {
    Client client;
    Renderer renderer;
    QString loadedIdentity;
    for (;;) {
        ThumbnailRequest request;
        quint64 requestCancellation;
        {
            QMutexLocker lock(&mutex);
            if (!requested && !stopping) {
                if (!pending.wait(&mutex, 10000)) {
                    renderer.reset();
                    client.reset();
                    loadedIdentity.clear();
                }
            }
            if (stopping)
                return;
            if (!requested)
                continue;
            request = pendingRequest;
            requestCancellation = cancellation;
            requested = false;
        }
        if (!client) {
            client.reset(mpv_create());
            if (!client)
                continue;
            for (const auto &[name, value] : {std::pair{"config", "no"},
                                              {"vo", "libmpv"},
                                              {"audio", "no"},
                                              {"sid", "no"},
                                              {"hwdec", "no"},
                                              {"vd-lavc-threads", "1"},
                                              {"pause", "yes"},
                                              {"keep-open", "yes"},
                                              {"demuxer-max-bytes", "4194304"},
                                              {"demuxer-max-back-bytes", "0"}})
                mpv_set_option_string(client.get(), name, value);
            if (mpv_initialize(client.get()) < 0) {
                client.reset();
                continue;
            }
            mpv_render_param parameters[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)},
                {MPV_RENDER_PARAM_INVALID, nullptr}};
            mpv_render_context *context = nullptr;
            if (mpv_render_context_create(&context, client.get(), parameters) < 0) {
                client.reset();
                continue;
            }
            renderer.reset(context);
        }
        const bool load = loadedIdentity != request.fileIdentity;
        if (load) {
            // Start near the requested position instead of first decoding the beginning.
            const auto start = QByteArray::number(request.second);
            mpv_set_property_string(client.get(), "start", start.constData());
            loadedIdentity = request.fileIdentity;
            const auto encoded = request.path.toUtf8();
            const char *arguments[]{"loadfile", encoded.constData(), nullptr};
            mpv_command_async(client.get(), 0, arguments);
        } else {
            // Drain completed events from the previous seek before accepting this one's restart.
            while (mpv_wait_event(client.get(), 0)->event_id != MPV_EVENT_NONE) {
            }
        }
        bool delivered = false;
        bool seekSent = false;
        bool seekStarted = false;
        QElapsedTimer timeout;
        timeout.start();
        while (timeout.elapsed() < 5000) {
            {
                QMutexLocker lock(&mutex);
                if (stopping)
                    return;
                if (requested || cancellation != requestCancellation)
                    break;
            }
            const auto *event = mpv_wait_event(client.get(), 0.02);
            if ((!load || event->event_id == MPV_EVENT_FILE_LOADED) && !seekSent) {
                const auto target = QByteArray::number(request.second);
                const char *arguments[]{"seek", target.constData(), request.precise ? "absolute+exact" : "absolute+keyframes", nullptr};
                mpv_command_async(client.get(), 0, arguments);
                seekSent = true;
            }
            if (seekSent && event->event_id == MPV_EVENT_SEEK)
                seekStarted = true;
            // Rendering also drains the frame queue while paused, allowing decoder initialization.
            if (mpv_render_context_update(renderer.get()) & MPV_RENDER_UPDATE_FRAME) {
                auto frame = renderThumbnail(renderer.get());
                if (seekStarted && event->event_id == MPV_EVENT_PLAYBACK_RESTART && !frame.isNull()) {
                    emit ready(frame, request);
                    delivered = true;
                    break;
                }
            }
            if (seekStarted && event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
                const auto frame = renderThumbnail(renderer.get());
                if (!frame.isNull()) {
                    emit ready(frame, request);
                    delivered = true;
                }
                break;
            }
            if (event->event_id == MPV_EVENT_END_FILE)
                break;
        }
        if (!delivered)
            emit ready({}, request);
    }
}
ThumbnailService::ThumbnailService(QObject *parent)
    : QObject(parent), ownedWorker(std::make_unique<ThumbnailWorker>()), worker(*ownedWorker) {
    initialize();
}
ThumbnailService::ThumbnailService(ThumbnailWorker &worker, QObject *parent)
    : QObject(parent), worker(worker) {
    initialize();
}
void ThumbnailService::initialize() {
    qRegisterMetaType<ThumbnailRequest>();
    throttle.setSingleShot(true);
    throttle.setInterval(50);
    refinement.setSingleShot(true);
    refinement.setInterval(180);
    connect(&throttle, &QTimer::timeout, this, [this] { dispatch(false); });
    connect(&refinement, &QTimer::timeout, this, [this] { dispatch(true); });
    connect(&worker, &ThumbnailWorker::ready, this, &ThumbnailService::acceptImage);
}
QString ThumbnailService::imageUrl() const {
    return image.isNull() ? QString() : "image://preview/" + QString::number(imageRevision);
}
void ThumbnailService::displayImage(const QImage &frame, bool approximate) {
    image = frame;
    imageApproximate = approximate;
    ++imageRevision;
    emit changed();
}
void ThumbnailService::request(const QString &path, double seconds) {
    const QFileInfo file(path);
    if (!std::isfinite(seconds) || seconds < 0 || seconds > INT_MAX || !file.isFile()) {
        clear();
        return;
    }
    // Include file identity so replacement/editing cannot reuse stale previews.
    const auto identityBytes = file.canonicalFilePath().toUtf8() + '\0'
        + QByteArray::number(file.size()) + ':' + QByteArray::number(file.lastModified().toMSecsSinceEpoch());
    const QString identity = QString::fromLatin1(QCryptographicHash::hash(identityBytes, QCryptographicHash::Sha256).toHex());
    const int second = static_cast<int>(std::floor(seconds));
    refinement.start();
    if (active && target.fileIdentity == identity && target.second == second)
        return;
    const bool newFile = target.fileIdentity != identity;
    target = {file.canonicalFilePath(), identity, second, false, target.generation + 1};
    active = true;
    if (newFile) {
        worker.cancel();
        lastDispatchedKey.clear();
        throttle.stop();
        displayImage({}, false);
    }
    auto exactRequest = target;
    exactRequest.precise = true;
    if (const auto *cached = cache.object(exactRequest.cacheKey()))
        displayImage(*cached, false);
    else if (const auto *cached = cache.object(target.cacheKey()))
        displayImage(*cached, true);
    else if (!image.isNull()) {
        // Keep the previous image while loading, with an explicit approximate indicator.
        imageApproximate = true;
        emit changed();
    }
    if (!throttle.isActive()) {
        dispatch(false);
        throttle.start();
    }
}
void ThumbnailService::dispatch(bool precise) {
    if (!active)
        return;
    auto request = target;
    request.precise = true;
    if (cache.object(request.cacheKey()))
        return;
    request.precise = precise;
    if (cache.object(request.cacheKey()) || lastDispatchedKey == request.cacheKey())
        return;
    lastDispatchedKey = request.cacheKey();
    worker.request(request);
}
void ThumbnailService::acceptImage(const QImage &frame, const ThumbnailRequest &request) {
    if (frame.isNull()) {
        if (request.generation == target.generation && lastDispatchedKey == request.cacheKey())
            lastDispatchedKey.clear();
        return;
    }
    cache.insert(request.cacheKey(), new QImage(frame), static_cast<int>(frame.sizeInBytes()));
    if (!active || request.generation != target.generation)
        return;
    auto exactRequest = target;
    exactRequest.precise = true;
    if (!request.precise && cache.object(exactRequest.cacheKey()))
        return;
    displayImage(frame, !request.precise);
}
void ThumbnailService::clear() {
    active = false;
    ++target.generation;
    throttle.stop();
    refinement.stop();
    lastDispatchedKey.clear();
    worker.cancel();
    displayImage({}, false);
}
