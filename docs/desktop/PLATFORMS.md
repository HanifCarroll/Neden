# Platform support

macOS is the current development target. The player uses Qt Quick/OpenGL, an explicitly reviewed dynamically linked libmpv runtime, native WKWebView through Qt WebView, and macOS Keychain. Build with the persistent Apple Development identity to retain existing app-specific Keychain approvals. Native user interaction must be verified on a real display; headless tests do not establish fullscreen, text, focus, or WebView behavior.

Linux has a source-build recipe at `scripts/build_linux_runtime.sh` and an explicit `AUDITED_MPV_PREFIX` CMake option. The configuration check binds the reviewed mpv manifest to its binary and queries FFmpeg's embedded license. Qt WebView may use a Qt WebEngine backend, bringing its own dependencies and notices. Earlier container checks do not establish physical desktop playback or release support. The native credential adapter is still missing.

Windows has no reviewed playback runtime configuration or credential-store adapter. The full player build deliberately fails rather than selecting an arbitrary mpv binary. Portable core tests can be built using `NEDEN_CORE_ONLY=ON` on a compatible Qt toolchain.

Before claiming Windows or Linux support, verify a reviewed runtime and distributable package, hardware decoding and software fallback, actual visible playback, dual Turkish/English subtitles, Turkish search, dictionary navigation and cleanup, explanation streaming/cancellation with mock credentials, settings shortcuts, fullscreen and focus, and local card persistence. Containers can validate builds and pure logic; they cannot establish representative GPU, display, or fullscreen performance.
