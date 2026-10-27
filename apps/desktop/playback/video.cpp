#include "video.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <array>
#include <mpv/render_gl.h>

Q_LOGGING_CATEGORY(videoRendering, "neden.video", QtWarningMsg)

class VideoRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit VideoRenderer(const Video *item)
        : window(item->window()), videoItem(item), playback(item->playback()) {
        swapConnection = QObject::connect(
            window, &QQuickWindow::frameSwapped, window,
            [this] {
                if (context)
                    mpv_render_context_report_swap(context);
            },
            Qt::DirectConnection);
    }
    ~VideoRenderer() override {
        QObject::disconnect(swapConnection);
        if (!context)
            return;

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
        if (!buffer->isValid())
            qFatal("Cannot allocate the OpenGL video framebuffer.");

        return buffer;
    }
    void render() override {

        QQuickOpenGLUtils::resetOpenGLState();
        auto *functions = QOpenGLContext::currentContext()->functions();
        const bool inheritedFaceCulling = videoRendering().isDebugEnabled() && functions->glIsEnabled(GL_CULL_FACE);
        // Qt's RHI can enable face culling, but resetOpenGLState does not
        // restore it. libmpv requires its standard disabled state; otherwise
        // Qt's winding/cull mode can discard the video quad entirely.
        functions->glDisable(GL_CULL_FACE);

        // 1. Allocate mpv's GPU resources only when a media request needs them.
        if (!context && outputRequested)
            initializeContext();

        if (!context) {
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
        const int result = mpv_render_context_render(context, parameters);
        if (result < 0)
            qCWarning(videoRendering) << "Video rendering failed:" << mpv_error_string(result);
        if (videoRendering().isDebugEnabled() && (++renderedFrames == 1 || renderedFrames % 60 == 0)) {
            // Opt-in diagnostics separate decoder/FBO failures from presentation
            // failures. Never read back pixels during normal playback.
            GLint previousFramebuffer = 0;
            functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
            functions->glBindFramebuffer(GL_FRAMEBUFFER, target->handle());
            std::array<GLubyte, 4> center{};
            functions->glReadPixels(target->width() / 2, target->height() / 2, 1, 1,
                                    GL_RGBA, GL_UNSIGNED_BYTE, center.data());
            const auto error = functions->glGetError();
            functions->glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
            qCDebug(videoRendering) << "Frame" << renderedFrames << "FBO" << target->size()
                << "center RGBA" << center[0] << center[1] << center[2] << center[3]
                << "GL error" << error << "mpv result" << result
                << "inherited face culling" << inheritedFaceCulling;
        }
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    void initializeContext() {

        // 1. Bind libmpv to the scene graph's current OpenGL context.
        mpv_opengl_init_params initialization{[](void *, const char *name) -> void * {
                                                  return reinterpret_cast<void *>(
                                                      QOpenGLContext::currentContext()->getProcAddress(name));
                                              },
                                              nullptr};
        mpv_render_param parameters[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initialization},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        if (mpv_render_context_create(&context, playback->handle(), parameters) < 0) {
            qFatal("Cannot initialize libmpv OpenGL rendering.");
        }

        // 2. Publish ownership only after the context can accept a media load.
        mpv_render_context_set_update_callback(
            context,
            [](void *data) {
                QMetaObject::invokeMethod(static_cast<Video *>(data), "update", Qt::QueuedConnection);
            },
            const_cast<Video *>(videoItem));
        renderToken = playback->attachVideoRenderer();
    }

    QQuickWindow *window;
    const Video *videoItem;
    MpvPlayback *playback;
    VideoRenderLifetime::Token renderToken = 0;
    bool outputRequested = false;
    bool hasVideo = false;
    quint64 renderedFrames = 0;
    mpv_render_context *context = nullptr;
    QMetaObject::Connection swapConnection;
};
QQuickFramebufferObject::Renderer *Video::createRenderer() const {
    return new VideoRenderer(this);
}
