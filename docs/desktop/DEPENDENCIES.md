# Desktop dependencies and distribution

Neden's application source is MIT licensed. The linked libraries retain their own licenses; a closed-source distribution still needs to satisfy those licenses.

## Qt and fonts

The Mac build uses dynamically linked Qt 6.12 Core, Gui, Network, OpenGL, Qml, Quick, Quick Controls 2, and WebView under their LGPLv3 option, or a separately obtained commercial Qt license. Qt Test is development-only. No GPL-only Qt module is intentionally selected. macOS Qt WebView uses Apple's WKWebView rather than shipping Chromium. The installer stages Qt's native WebView plugin and removes the optional `depends QtWebEngine auto` line from the bundled `QtWebView/qmldir`; this metadata-only adjustment is reproducible in `scripts/install_desktop.py`. Linux's WebView backend can involve Qt WebEngine and its additional third-party obligations.

Inter 4.1 Regular, Medium, and SemiBold are from [Inter's release](https://github.com/rsms/inter/releases/tag/v4.1), under SIL OFL 1.1. Complete notices are preserved at `shared/fonts/LICENSE.txt` and included as application resources. Original vector control marks are drawn locally. System fallback supplies scripts outside Inter's coverage.

## Reviewed macOS playback runtime

The pinned upstream package is [libmpv-darwin-build v0.6.0](https://github.com/media-kit/libmpv-darwin-build/tree/v0.6.0), `macos-universal-video-default`. Its XCFramework archive SHA-256 is `84d2ad98e046e82c6dc34d8547d76c2afeaee89c0f53032773be8985c95536d6`. The fetch script verifies that hash and selects only the macOS frameworks.

- mpv 0.36.0 is built with `-Dgpl=false`, dynamically linked, using its LGPL-2.1-or-later option.
- The six FFmpeg frameworks report **LGPL version 3 or later** and `--enable-version3`. GPL/nonfree code and libx264/libx265 are not enabled. Use the actual embedded LGPLv3 designation rather than the build README's shorthand.
- Supporting libraries include libass (ISC), FreeType (FreeType License option), HarfBuzz (MIT), FriBidi (LGPL), Mbed TLS (Apache-2.0), uchardet (MPL/LGPL option), libxml2 (MIT), dav1d (BSD-2-Clause), and libpng (libpng license).

`scripts/check_playback_licenses.py` rejects missing LGPL evidence, GPL/nonfree configuration, and dependencies outside the reviewed 18-framework set or Apple system libraries. This is a regression guard for this specific build, not a general license scanner. libmpv headers used during compilation have their own ISC notices; linking an arbitrary Homebrew mpv binary is deliberately disallowed.

The Linux recipe pins FFmpeg 8.0.1 and mpv 0.41.0, disabling GPL/nonfree components. Its companion guard queries FFmpeg licenses and validates the mpv binary against the source-build manifest. It does not establish complete Linux distribution compliance.

## Before distributing

Package complete dependency notices, corresponding LGPL library sources with exact patches and build instructions, and the information needed to replace/relink modified libraries, including signing/install steps. App terms must preserve the debugging/reverse-engineering freedoms required by those licenses. This does not by itself require publishing Neden's application source. Final packaging, store terms, and applicable codec patent questions depend on the distribution route.

The local installation command embeds Qt, QML plugins, and reviewed playback libraries and checks for external library paths. Full notices/source packages and library replacement instructions have not been assembled for public distribution, so the local bundle is **not distribution-ready**. Required notices and license checks are part of the necessary project files.

Primary references: [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations), [Qt licensing](https://doc.qt.io/qt-6/licensing.html), [mpv 0.36.0 copyright](https://github.com/mpv-player/mpv/blob/v0.36.0/Copyright), and [FFmpeg legal checklist](https://ffmpeg.org/legal.html).

Focused implementation references included Qt's scene graph/OpenGL documentation, libmpv's render headers, and the OpenGL context requirements in media_kit. Jellyfin Desktop provided a lead for declaring an sRGB surface. No implementation from those projects was copied into the app.
