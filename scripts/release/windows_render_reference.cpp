// Standalone diagnostic for the installed libmpv + Mesa pair. No Qt or Neden UI.
// Follows the event/render separation in mpv-examples/libmpv/sdl/main.c;
// uses Win32 directly so the probe adds no third-party runtime dependency.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <utility>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

template <typename Function>
Function loadFunction(HMODULE library, const char *name) {
    auto function = reinterpret_cast<Function>(GetProcAddress(library, name));
    if (!function)
        throw std::runtime_error(std::string("Missing library function: ") + name);
    return function;
}

struct GraphicsLibrary {
    HMODULE library;
    PROC (WINAPI *getExtension)(LPCSTR);
    explicit GraphicsLibrary(const std::filesystem::path &path) : library(LoadLibraryW(path.c_str())) {
        if (!library)
            throw std::runtime_error("Could not load graphics library");
        getExtension = loadFunction<decltype(getExtension)>(library, "wglGetProcAddress");
    }
    static void *resolve(void *data, const char *name) {
        auto &self = *static_cast<GraphicsLibrary *>(data);
        auto function = self.getExtension(name);
        const auto address = reinterpret_cast<std::intptr_t>(function);
        if (!function || address == 1 || address == 2 || address == 3 || address == -1)
            function = GetProcAddress(self.library, name);
        return reinterpret_cast<void *>(function);
    }
    template <typename Function> Function function(const char *name) {
        auto result = reinterpret_cast<Function>(resolve(this, name));
        if (!result)
            throw std::runtime_error(std::string("Missing graphics function: ") + name);
        return result;
    }
};

COLORREF presentedPixel(HWND window, int x, int y) {
    POINT position{x, y};
    if (!ClientToScreen(window, &position))
        return CLR_INVALID;
    const auto screen = GetDC(nullptr);
    const auto copy = CreateCompatibleDC(screen);
    const auto bitmap = CreateCompatibleBitmap(screen, 1, 1);
    const auto previous = SelectObject(copy, bitmap);
    const bool captured = BitBlt(copy, 0, 0, 1, 1, screen, position.x, position.y, SRCCOPY | CAPTUREBLT);
    const auto pixel = captured ? GetPixel(copy, 0, 0) : CLR_INVALID;
    SelectObject(copy, previous);
    DeleteObject(bitmap);
    DeleteDC(copy);
    ReleaseDC(nullptr, screen);
    return pixel;
}

bool isWhite(COLORREF pixel) {
    return pixel != CLR_INVALID && GetRValue(pixel) >= 220
        && GetGValue(pixel) >= 220 && GetBValue(pixel) >= 220;
}

int main(int argc, char **argv) try {
    if (argc != 5)
        throw std::runtime_error("Usage: reference.exe MPV_DLL OPENGL_DLL WHITE_VIDEO MODE");
    const std::string mode = argv[4];
    if (mode != "default" && mode != "rgb" && mode != "simple")
        throw std::runtime_error("Unknown reference mode");
    GraphicsLibrary graphics{std::filesystem::path(argv[2])};
    auto playbackLibrary = LoadLibraryA(argv[1]);
    if (!playbackLibrary)
        throw std::runtime_error("Could not load playback library");
    const auto create = loadFunction<decltype(&mpv_create)>(playbackLibrary, "mpv_create");
    const auto setOption = loadFunction<decltype(&mpv_set_option_string)>(playbackLibrary, "mpv_set_option_string");
    const auto initialize = loadFunction<decltype(&mpv_initialize)>(playbackLibrary, "mpv_initialize");
    const auto command = loadFunction<decltype(&mpv_command_async)>(playbackLibrary, "mpv_command_async");
    const auto waitEvent = loadFunction<decltype(&mpv_wait_event)>(playbackLibrary, "mpv_wait_event");
    const auto destroy = loadFunction<decltype(&mpv_terminate_destroy)>(playbackLibrary, "mpv_terminate_destroy");
    const auto createRender = loadFunction<decltype(&mpv_render_context_create)>(playbackLibrary, "mpv_render_context_create");
    const auto setCallback = loadFunction<decltype(&mpv_render_context_set_update_callback)>(playbackLibrary, "mpv_render_context_set_update_callback");
    const auto updateRender = loadFunction<decltype(&mpv_render_context_update)>(playbackLibrary, "mpv_render_context_update");
    const auto render = loadFunction<decltype(&mpv_render_context_render)>(playbackLibrary, "mpv_render_context_render");
    const auto reportSwap = loadFunction<decltype(&mpv_render_context_report_swap)>(playbackLibrary, "mpv_render_context_report_swap");
    const auto freeRender = loadFunction<decltype(&mpv_render_context_free)>(playbackLibrary, "mpv_render_context_free");

    WNDCLASSW windowClass{};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"NedenRenderReference";
    if (!RegisterClassW(&windowClass))
        throw std::runtime_error("Could not register window");
    const auto window = CreateWindowW(windowClass.lpszClassName, L"libmpv reference diagnostic",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 800, 600, nullptr, nullptr, windowClass.hInstance, nullptr);
    const auto device = GetDC(window);
    PIXELFORMATDESCRIPTOR pixelFormat{};
    pixelFormat.nSize = sizeof(pixelFormat);
    pixelFormat.nVersion = 1;
    pixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormat.iPixelType = PFD_TYPE_RGBA;
    pixelFormat.cColorBits = 32;
    const auto chooseFormat = loadFunction<int (WINAPI *)(HDC, const PIXELFORMATDESCRIPTOR *)>(graphics.library, "wglChoosePixelFormat");
    const auto setFormat = loadFunction<BOOL (WINAPI *)(HDC, int, const PIXELFORMATDESCRIPTOR *)>(graphics.library, "wglSetPixelFormat");
    const auto createContext = loadFunction<HGLRC (WINAPI *)(HDC)>(graphics.library, "wglCreateContext");
    const auto makeCurrent = loadFunction<BOOL (WINAPI *)(HDC, HGLRC)>(graphics.library, "wglMakeCurrent");
    const auto deleteContext = loadFunction<BOOL (WINAPI *)(HGLRC)>(graphics.library, "wglDeleteContext");
    const auto swapBuffers = loadFunction<BOOL (WINAPI *)(HDC)>(graphics.library, "wglSwapBuffers");
    if (!setFormat(device, chooseFormat(device, &pixelFormat), &pixelFormat))
        throw std::runtime_error("Could not set pixel format");
    const auto bootstrapContext = createContext(device);
    if (!makeCurrent(device, bootstrapContext))
        throw std::runtime_error("Could not make bootstrap context current");
    const auto createCoreContext = graphics.function<HGLRC (WINAPI *)(HDC, HGLRC, const int *)>("wglCreateContextAttribsARB");
    // WGL_CONTEXT_MAJOR_VERSION_ARB, MINOR_VERSION_ARB, PROFILE_MASK_ARB/core.
    const int attributes[] = {0x2091, 3, 0x2092, 2, 0x9126, 1, 0};
    const auto context = createCoreContext(device, nullptr, attributes);
    if (!context || !makeCurrent(device, context))
        throw std::runtime_error("Could not create core context");
    deleteContext(bootstrapContext);
    const auto getString = graphics.function<const GLubyte *(APIENTRY *)(GLenum)>("glGetString");
    std::printf("Renderer: %s; version: %s\n", getString(GL_RENDERER), getString(GL_VERSION));

    const auto genTextures = graphics.function<void (APIENTRY *)(GLsizei, GLuint *)>("glGenTextures");
    const auto bindTexture = graphics.function<void (APIENTRY *)(GLenum, GLuint)>("glBindTexture");
    const auto textureImage = graphics.function<void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>("glTexImage2D");
    const auto genFramebuffers = graphics.function<void (APIENTRY *)(GLsizei, GLuint *)>("glGenFramebuffers");
    const auto bindFramebuffer = graphics.function<void (APIENTRY *)(GLenum, GLuint)>("glBindFramebuffer");
    const auto attachTexture = graphics.function<void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint)>("glFramebufferTexture2D");
    const auto framebufferStatus = graphics.function<GLenum (APIENTRY *)(GLenum)>("glCheckFramebufferStatus");
    const auto readPixels = graphics.function<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>("glReadPixels");
    const auto blitFramebuffer = graphics.function<void (APIENTRY *)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)>("glBlitFramebuffer");
    constexpr GLenum framebufferTarget = 0x8D40, readFramebuffer = 0x8CA8, drawFramebuffer = 0x8CA9;
    RECT bounds{};
    GetClientRect(window, &bounds);
    const int width = bounds.right, height = bounds.bottom;
    GLuint texture = 0, framebuffer = 0;
    genTextures(1, &texture);
    bindTexture(GL_TEXTURE_2D, texture);
    textureImage(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    bindTexture(GL_TEXTURE_2D, 0);
    genFramebuffers(1, &framebuffer);
    bindFramebuffer(framebufferTarget, framebuffer);
    attachTexture(framebufferTarget, 0x8CE0, GL_TEXTURE_2D, texture, 0);
    if (framebufferStatus(framebufferTarget) != 0x8CD5)
        throw std::runtime_error("Incomplete framebuffer");
    bindFramebuffer(framebufferTarget, 0);

    // Validate the probe's own presentation/capture path before testing mpv.
    SetForegroundWindow(window);
    const auto clearColor = graphics.function<void (APIENTRY *)(GLfloat, GLfloat, GLfloat, GLfloat)>("glClearColor");
    const auto clear = graphics.function<void (APIENTRY *)(GLbitfield)>("glClear");
    clearColor(1, 1, 1, 1);
    clear(GL_COLOR_BUFFER_BIT);
    swapBuffers(device);
    Sleep(100);
    const auto controlPixel = presentedPixel(window, width / 2, height / 2);
    std::printf("Presentation control: %u,%u,%u\n", GetRValue(controlPixel), GetGValue(controlPixel), GetBValue(controlPixel));
    if (!isWhite(controlPixel))
        throw std::runtime_error("Probe presentation/capture control failed before mpv initialization");
    clearColor(0, 0, 0, 1);

    auto *playback = create();
    if (!playback)
        throw std::runtime_error("Could not create playback");
    for (const auto &[name, value] : std::array{
             std::pair{"config", "no"}, std::pair{"vo", "libmpv"}, std::pair{"hwdec", "no"},
             std::pair{"terminal", "yes"}, std::pair{"msg-level", "all=v"},
             std::pair{"video-timing-offset", "0"}}) {
        if (setOption(playback, name, value) < 0)
            throw std::runtime_error("Could not set playback option");
    }
    // Isolate color conversion from intermediate shader passes. These options
    // are diagnostic only and never change the installed application's settings.
    if (mode == "rgb" && setOption(playback, "vf", "format=fmt=rgba") < 0)
        throw std::runtime_error("Could not request CPU color conversion");
    if (mode == "simple" && setOption(playback, "gpu-dumb-mode", "yes") < 0)
        throw std::runtime_error("Could not request single-pass rendering");
    std::printf("Reference mode: %s\n", mode.c_str());
    if (initialize(playback) < 0)
        throw std::runtime_error("Could not initialize playback");
    mpv_opengl_init_params graphicsParameters{GraphicsLibrary::resolve, &graphics};
    mpv_render_param contextParameters[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &graphicsParameters}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context *renderer = nullptr;
    if (createRender(&renderer, playback, contextParameters) < 0)
        throw std::runtime_error("Could not create renderer");
    std::atomic_bool frameRequested = true;
    setCallback(renderer, [](void *data) { static_cast<std::atomic_bool *>(data)->store(true); }, &frameRequested);
    const char *loadArguments[] = {"loadfile", argv[3], nullptr};
    if (command(playback, 0, loadArguments) < 0)
        throw std::runtime_error("Could not load fixture");
    SetForegroundWindow(window);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    int whiteFrames = 0, frames = 0;
    while (std::chrono::steady_clock::now() < deadline && whiteFrames < 10) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        while (waitEvent(playback, 0)->event_id != MPV_EVENT_NONE) {}
        if (!frameRequested.exchange(false)) {
            Sleep(5);
            continue;
        }
        updateRender(renderer);
        mpv_opengl_fbo target{static_cast<int>(framebuffer), width, height, GL_RGBA8};
        int flip = 1, block = 0;
        mpv_render_param renderParameters[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target}, {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        if (render(renderer, renderParameters) < 0)
            throw std::runtime_error("Render failed");
        bindFramebuffer(framebufferTarget, framebuffer);
        std::array<GLubyte, 4> pixel{};
        readPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        bindFramebuffer(readFramebuffer, framebuffer);
        bindFramebuffer(drawFramebuffer, 0);
        blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        bindFramebuffer(framebufferTarget, 0);
        swapBuffers(device);
        reportSwap(renderer);
        const auto visiblePixel = presentedPixel(window, width / 2, height / 2);
        const bool white = pixel[0] >= 220 && pixel[1] >= 220 && pixel[2] >= 220
            && isWhite(visiblePixel);
        whiteFrames = white ? whiteFrames + 1 : 0;
        if (++frames <= 3 || frames % 60 == 0)
            std::printf("Frame %d: framebuffer=%u,%u,%u screen=%u,%u,%u\n", frames,
                pixel[0], pixel[1], pixel[2], GetRValue(visiblePixel), GetGValue(visiblePixel), GetBValue(visiblePixel));
    }
    setCallback(renderer, nullptr, nullptr);
    freeRender(renderer);
    destroy(playback);
    makeCurrent(nullptr, nullptr);
    deleteContext(context);
    ReleaseDC(window, device);
    DestroyWindow(window);
    std::printf("Consecutive visible white frames: %d\n", whiteFrames);
    return whiteFrames == 10 ? 0 : 4;
} catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 2;
}
