#include "thumbnail_service.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <mpv/client.h>
#include <mpv/render.h>
#include <memory>
#include <cmath>

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
void ThumbnailWorker::request(const QString &path, double seconds, quint64 requestGeneration) {
    QMutexLocker lock(&mutex);
    mediaPath = path;
    position = seconds;
    generation = requestGeneration;
    requested = true;
    if (!isRunning())
        start();
    pending.wakeOne();
}
void ThumbnailWorker::run() {
    Client client;
    Renderer renderer;
    QString loadedPath;
    for (;;) {
        QString path;
        double seconds;
        quint64 requestGeneration;
        {
            QMutexLocker lock(&mutex);
            if (!requested && !stopping) {
                if (!pending.wait(&mutex, 10000)) {
                    renderer.reset();
                    client.reset();
                    loadedPath.clear();
                }
            }
            if (stopping)
                return;
            if (!requested)
                continue;
            path = mediaPath;
            seconds = position;
            requestGeneration = generation;
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
        const bool load = loadedPath != path;
        if (load) {
            loadedPath = path;
            const auto encoded = path.toUtf8();
            const char *arguments[]{"loadfile", encoded.constData(), nullptr};
            mpv_command_async(client.get(), 0, arguments);
        } else {
            // Drain completed events from the previous seek before accepting this one's restart.
            while (mpv_wait_event(client.get(), 0)->event_id != MPV_EVENT_NONE) {
            }
        }
        bool seekSent = false;
        bool seekStarted = false;
        QElapsedTimer timeout;
        timeout.start();
        while (timeout.elapsed() < 5000) {
            {
                QMutexLocker lock(&mutex);
                if (stopping)
                    return;
                if (requested)
                    break;
            }
            const auto *event = mpv_wait_event(client.get(), 0.02);
            if ((!load || event->event_id == MPV_EVENT_FILE_LOADED) && !seekSent) {
                const auto target = QByteArray::number(seconds, 'f', 3);
                const char *arguments[]{"seek", target.constData(), "absolute+exact", nullptr};
                mpv_command_async(client.get(), 0, arguments);
                seekSent = true;
            }
            if (seekSent && event->event_id == MPV_EVENT_SEEK)
                seekStarted = true;
            // Rendering also drains the frame queue while paused, allowing decoder initialization.
            if (mpv_render_context_update(renderer.get()) & MPV_RENDER_UPDATE_FRAME) {
                auto frame = renderThumbnail(renderer.get());
                if (seekStarted && event->event_id == MPV_EVENT_PLAYBACK_RESTART && !frame.isNull()) {
                    emit ready(frame, requestGeneration);
                    break;
                }
            }
            if (seekStarted && event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
                const auto frame = renderThumbnail(renderer.get());
                if (!frame.isNull())
                    emit ready(frame, requestGeneration);
                break;
            }
            if (event->event_id == MPV_EVENT_END_FILE)
                break;
        }
    }
}
ThumbnailService::ThumbnailService(QObject *parent) : QObject(parent) {
    connect(&worker, &ThumbnailWorker::ready, this, [this](const QImage &frame, quint64 requestGeneration) {
        if (requestGeneration != generation)
            return;
        image = frame;
        emit changed();
    });
}
void ThumbnailService::request(const QString &path, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || !QFileInfo(path).isFile()) {
        clear();
        return;
    }
    const int second = static_cast<int>(std::floor(seconds));
    if (path == lastPath && second == lastSecond)
        return;
    lastPath = path;
    lastSecond = second;
    image = {};
    emit changed();
    worker.request(path, second, ++generation);
}
void ThumbnailService::clear() {
    ++generation;
    image = {};
    lastPath.clear();
    lastSecond = -1;
    emit changed();
}
