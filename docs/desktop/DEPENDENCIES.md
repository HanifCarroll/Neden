# Desktop dependencies and distribution

Neden's application source is MIT licensed. The linked libraries retain their own licenses; a closed-source distribution still needs to satisfy those licenses.

## Qt and fonts

The release workflow pins Qt 6.10.2; local builds may use newer compatible Qt versions. The Mac build uses dynamically linked Qt Core, Gui, Network, OpenGL, Qml, Quick, Quick Controls 2, Svg, and WebView under their LGPLv3 option, or a separately obtained commercial Qt license. Qt Test is development-only. No GPL-only Qt module is intentionally selected. macOS Qt WebView uses Apple's WKWebView rather than shipping Chromium. The installer stages Qt's native WebView plugin and removes the optional `depends QtWebEngine auto` line from the bundled `QtWebView/qmldir`; this metadata-only adjustment is reproducible in `scripts/install_desktop.py`. Linux’s packaged WebView uses Qt WebEngine, including its helper process, resources, locale data, and third-party notices/sources. Windows selects WebView2 and removes the unused Qt WebEngine plugin and QML dependency; WebView2 Evergreen is supplied by Microsoft separately.

Inter 4.1 Regular, Medium, and SemiBold are from [Inter's release](https://github.com/rsms/inter/releases/tag/v4.1), under SIL OFL 1.1. Complete notices are preserved at `shared/fonts/LICENSE.txt` and included as application resources. Control icons are from the pinned [Lucide repository](https://github.com/lucide-icons/lucide), under ISC. The source commit and color adaptation are recorded in `shared/icons/lucide/SOURCE.md`; the complete license is in `shared/icons/lucide/LICENSE` and is bundled by the installer. Qt's SVG image plugin renders those local icons. System fallback supplies scripts outside Inter's coverage.

## Reviewed macOS playback runtime

The pinned upstream package is [libmpv-darwin-build v0.6.0](https://github.com/media-kit/libmpv-darwin-build/tree/v0.6.0), `macos-universal-video-default`. Its XCFramework archive SHA-256 is `84d2ad98e046e82c6dc34d8547d76c2afeaee89c0f53032773be8985c95536d6`. The fetch script verifies that hash and selects only the macOS frameworks.

- mpv 0.36.0 is built with `-Dgpl=false`, dynamically linked, using its LGPL-2.1-or-later option.
- The six FFmpeg frameworks report **LGPL version 3 or later** and `--enable-version3`. GPL/nonfree code and libx264/libx265 are not enabled. Use the actual embedded LGPLv3 designation rather than the build README's shorthand.
- Supporting libraries include libass (ISC), FreeType (FreeType License option), HarfBuzz (MIT), FriBidi (LGPL), Mbed TLS (Apache-2.0), uchardet (MPL/LGPL option), libxml2 (MIT), dav1d (BSD-2-Clause), and libpng (libpng license).

`scripts/check_playback_licenses.py` rejects missing LGPL evidence, GPL/nonfree configuration, and dependencies outside the reviewed 18-framework set or Apple system libraries. This is a regression guard for this specific build, not a general license scanner. libmpv headers used during compilation have their own ISC notices; linking an arbitrary Homebrew mpv binary is deliberately disallowed.

The Linux and Windows recipes pin FFmpeg 8.0.1 and mpv 0.41.0, disabling GPL/nonfree components. The companion guards query FFmpeg licenses and validate the runtime against its build manifest. Distribution inventories are collected separately; Linux keeps system libraries as package-manager dependencies, while Windows bundles and inventories its exact MSYS2 DLL closure.

## Before distributing

Package complete dependency notices, corresponding LGPL library sources with exact patches and build instructions, and the information needed to replace/relink modified libraries, including signing/install steps. App terms must preserve the debugging/reverse-engineering freedoms required by those licenses. This does not by itself require publishing Neden's application source. Final packaging, store terms, and applicable codec patent questions depend on the distribution route.

The local installation command embeds Qt, QML plugins, and reviewed playback libraries and checks for external library paths. Public packaging additionally collects checksum-pinned source archives and their notices with `scripts/release/collect_sources.py`. Windows records the exact MSYS2 DLL package inventory, verifies signed matching source packages, and includes their notices. Microsoft release CRT DLLs are copied app-locally from the compiler’s redistributable directory; they remain under Microsoft’s license. Every public preview includes matching source archives, SHA-256 checksums, and [library replacement instructions](LIBRARY_REPLACEMENT.md). A local development installation alone is not a substitute for those distribution materials.

Primary references: [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations), [Qt licensing](https://doc.qt.io/qt-6/licensing.html), [mpv 0.36.0 copyright](https://github.com/mpv-player/mpv/blob/v0.36.0/Copyright), and [FFmpeg legal checklist](https://ffmpeg.org/legal.html).

Focused implementation references included Qt's scene graph/OpenGL documentation, libmpv's render headers, and the OpenGL context requirements in media_kit. Jellyfin Desktop provided a lead for declaring an sRGB surface. No implementation from those projects was copied into the app.

## Secure credentials

QtKeychain 0.17.0 is pinned to commit `875f77d9f61bd97fd84cca47ce3bc71186dfbd09` and archive SHA-256 `f367072fdf43551104cc718e43056d218d0c6d81c1b1758603fb3cd87144f8cc` in `apps/desktop/cmake/QtKeychain.cmake`. It is statically linked under its modified BSD license; the upstream COPYING notice is included in the Mac bundle. Qt remains dynamically linked. CMake downloads the verified source during the first full build; core-only tests remain independent of it.

Explicit Connect and Save actions use asynchronous QtKeychain jobs with insecure fallback disabled. macOS retains the existing IINA endpoint service and `api-key` / `login` account names. A small native, noninteractive read adapter preserves already-approved background access; it never prompts. Windows/Linux use the same QtKeychain interface and require explicit Connect after launch. Missing/locked backends produce a status message, never plaintext preferences. Windows and Linux credential backends are not covered by the preview’s installer/startup validation; those integrations remain experimental.

Embedded text decoding links the existing reviewed FFmpeg libraries directly. On macOS, CMake fetches checksum-pinned FFmpeg 6.0 source headers to match the bundled runtime and generates `avconfig.h`; no new codec binary is downloaded or shipped. Linux and Windows use headers and libraries from `AUDITED_MPV_PREFIX`.
