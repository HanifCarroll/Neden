# Platform support

macOS is the current development target. The player uses Qt Quick/OpenGL, an explicitly reviewed dynamically linked libmpv runtime, native WKWebView through Qt WebView, and macOS Keychain. Fresh builds use ad hoc development signing. A developer-owned signing certificate can retain Keychain approvals between rebuilds. Native user interaction must be verified on a real display; headless tests do not establish fullscreen, text, focus, or WebView behavior.

Linux has a source-build recipe at `scripts/build_linux_runtime.sh` and an explicit `AUDITED_MPV_PREFIX` CMake option. The configuration check binds the reviewed mpv manifest to its binary and queries FFmpeg's embedded license. Qt WebView may use a Qt WebEngine backend, bringing its own dependencies and notices. Earlier container checks do not establish physical desktop playback or release support. QtKeychain provides the credential interface, but its Linux backend still needs native validation.

Windows has no reviewed playback runtime configuration. QtKeychain provides the credential interface; its Windows backend still needs native validation. The full player build deliberately fails rather than selecting an arbitrary mpv binary. Portable core tests can be built using `NEDEN_CORE_ONLY=ON` on a compatible Qt toolchain.

Before claiming Windows or Linux support, verify a reviewed runtime and distributable package, hardware decoding and software fallback, actual visible playback, dual Turkish/English subtitles, Turkish search, dictionary navigation and cleanup, explanation streaming/cancellation with mock credentials, settings shortcuts, fullscreen and focus, and local card persistence. Containers can validate builds and pure logic; they cannot establish representative GPU, display, or fullscreen performance.

Platform media adapters now exist for macOS MediaPlayer/IOKit, Linux MPRIS/desktop portal, and Windows SMTC. Their presence does not establish release support: consult the [feature validation ledger](PLAYER_FEATURES.md), particularly native subtitle timing and untested platform/hardware cases.
