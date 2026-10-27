# Platform support

Preview downloads target the following environments:

| Platform | Package | Validation scope |
| --- | --- | --- |
| macOS 13+, Apple silicon | DMG | 11 test suites, disk-image/signature checks, separate-Mac startup and native UI checks |
| Windows 10 (1809)+ / 11, x86-64 | Per-user setup EXE | 11 test suites, dependency audit, installation/startup/uninstallation on Windows Server 2022 CI |
| Ubuntu 24.04, x86-64 | Debian package | 11 test suites, dependency audit, installation/startup/uninstallation under Xvfb |

The table describes the published `v0.2.0-preview.1` artifacts. See the
[October 27 development acceptance pass](validation-2026-10-27.md) for newer checks
and unresolved findings; these do not retroactively qualify the published binaries.

New source builds require Windows 10 1903+ for the system ICU API; the table above
retains the requirements of the already-published preview.

Windows and Linux are experimental. OS requirements otherwise follow Qt 6.10; the actual test
machines and limitations are recorded with each release. These checks do not
establish physical GPU, audio, accessibility, Wayland, credential-backend, or
fullscreen behavior across those platforms. Intel Macs and ARM Windows/Linux are
not included in the current preview.

macOS is the primary development target. The player uses Qt Quick/OpenGL, an explicitly reviewed dynamically linked libmpv runtime, native WKWebView through Qt WebView, and macOS Keychain. Fresh builds use ad hoc development signing. A developer-owned signing certificate can retain Keychain approvals between rebuilds. Native user interaction must be verified on a real display; headless tests do not establish fullscreen, text, focus, or WebView behavior.

Linux has a source-build recipe at `scripts/build_linux_runtime.sh` and an explicit `AUDITED_MPV_PREFIX` CMake option. The configuration check binds the reviewed mpv manifest to its binary and queries FFmpeg's embedded license. Qt WebView may use a Qt WebEngine backend, bringing its own dependencies and notices. Earlier container checks do not establish physical desktop playback or release support. QtKeychain provides the credential interface. Development CI now verifies synthetic-key save/read/delete against Secret Service; locked-store and desktop-session behavior remain unqualified.

Windows has an experimental source-build recipe in `scripts/release/build_windows_runtime.sh`.
It builds shared FFmpeg and libmpv in MSYS2 UCRT64 with GPL/nonfree components
disabled, then generates the C import library for the MSVC/Qt application.
`AUDITED_MPV_PREFIX` selects this runtime explicitly; an arbitrary system mpv
binary is rejected. QtKeychain provides the credential interface. Development CI now verifies
synthetic-key save/read/delete against Windows Credential Manager; real-desktop
and upgrade behavior remain unqualified. Portable core tests can be built using
`NEDEN_CORE_ONLY=ON` on a compatible Qt toolchain.

Before promoting Windows or Linux beyond experimental previews, verify a reviewed runtime and distributable package, hardware decoding and software fallback, actual visible playback, dual subtitles in user-selected languages, multilingual search and word selection (including right-to-left and unspaced scripts), word-card navigation and cleanup, explanation streaming/cancellation with mock credentials, settings shortcuts, fullscreen and focus, and local card persistence. Containers can validate builds and pure logic; they cannot establish representative GPU, display, or fullscreen performance.

Platform media adapters now exist for macOS MediaPlayer/IOKit, Linux MPRIS/desktop portal, and Windows SMTC. Their presence does not establish release support: consult the [feature validation ledger](PLAYER_FEATURES.md), particularly native subtitle timing and untested platform/hardware cases.
