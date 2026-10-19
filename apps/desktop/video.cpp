#include "video.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <mpv/render_gl.h>

class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit VideoRenderer(const Video *item)
        : window(item->window()), videoItem(item), playback(item->playback()) {
        swapConnection = QObject::connect(window, &QQuickWindow::frameSwapped, window, [this] {
            if (context) mpv_render_context_report_swap(context);
        }, Qt::DirectConnection);
    }
    ~VideoRenderer() override {
        QObject::disconnect(swapConnection);
        if (!context) return;

        playback->detachVideoRenderer(renderToken);
        mpv_render_context_set_update_callback(context, nullptr, nullptr);
        mpv_render_context_free(context);
    }
    void synchronize(QQuickFramebufferObject *object) override {

        // 1. Read GUI-owned metadata only while Qt blocks its GUI thread.
        const auto *item = static_cast<Video *>(object);
        outputRequested = item->playback()->videoOutputRequested();
        const bool videoAvailable = !item->playback()->sourceOutputSize().isEmpty();
        if (hasVideo != videoAvailable) {
            hasVideo = videoAvailable;
            invalidateFramebufferObject();
        }
    }
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        // A color texture is sufficient for mpv; it does not use depth or stencil.
        const QSize selectedSize = hasVideo ? size : QSize(64, 64);
        auto *buffer = new QOpenGLFramebufferObject(selectedSize, QOpenGLFramebufferObject::NoAttachment);
        if (!buffer->isValid()) qFatal("Cannot allocate the OpenGL video framebuffer.");

        return buffer;
    }
    void render() override {

        // 1. Allocate mpv's GPU resources only when a media request needs them.
        if (!context && outputRequested) initializeContext();

        QQuickOpenGLUtils::resetOpenGLState();
        if (!context) {
            auto *functions = QOpenGLContext::currentContext()->functions();
            functions->glClearColor(0, 0, 0, 1);
            functions->glClear(GL_COLOR_BUFFER_BIT);
            QQuickOpenGLUtils::resetOpenGLState();
            return;
        }

        // 2. Render without sleeping on Qt's GUI thread. The playback adapter
        // pairs this with video-timing-offset=0 to preserve mpv's A/V timing.
        auto *target = framebufferObject();
        mpv_opengl_fbo framebuffer{static_cast<int>(target->handle()), target->width(), target->height(), 0};
        int flip = 1;
        int blockForTargetTime = 0;
        mpv_render_param parameters[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blockForTargetTime},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_update(context);
        mpv_render_context_render(context, parameters);
        QQuickOpenGLUtils::resetOpenGLState();
    }
private:
    void initializeContext() {

        // 1. Bind libmpv to the scene graph's current OpenGL context.
        mpv_opengl_init_params initialization{
            [](void *, const char *name) -> void * {
                return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(name));
            }, nullptr};
        mpv_render_param parameters[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initialization},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        if (mpv_render_context_create(&context, playback->handle(), parameters) < 0) {
            qFatal("Cannot initialize libmpv OpenGL rendering.");
        }

        // 2. Publish ownership only after the context can accept a media load.
        mpv_render_context_set_update_callback(context, [](void *data) {
            QMetaObject::invokeMethod(static_cast<Video *>(data), "update", Qt::QueuedConnection);
        }, const_cast<Video *>(videoItem));
        renderToken = playback->attachVideoRenderer();
    }

    QQuickWindow *window;
    const Video *videoItem;
    MpvPlayback *playback;
    VideoRenderLifetime::Token renderToken = 0;
    bool outputRequested = false;
    bool hasVideo = false;
    mpv_render_context *context = nullptr;
    QMetaObject::Connection swapConnection;
};
QQuickFramebufferObject::Renderer *Video::createRenderer() const { return new VideoRenderer(this); }
