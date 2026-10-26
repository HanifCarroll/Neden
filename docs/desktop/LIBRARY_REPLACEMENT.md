# Dependency sources and library replacement

Neden's source is MIT licensed. Its dependencies keep their original licenses.
No Neden terms restrict reverse engineering for debugging modifications to those
libraries. Qt and playback libraries are dynamically linked. QtKeychain is linked
under its BSD license. Font and icon notices accompany the application.

Each release must include the exact dependency source archives, upstream patches,
build recipes, notices, and a SHA-256 manifest beside its installers. The source
collector preserves archives without modifying them. Its `notices` directory
contains upstream license and copyright files in their original directory layout.
Linux system libraries installed by the package manager remain separate packages.
Windows package-manager DLLs require their matching source packages and notices
in addition to the explicitly pinned archives.

## Rebuilding

Use the release tag's desktop build guide and `.github/workflows/desktop-packages.yml`.
The workflows record the Qt version and toolchains. The Linux and Windows playback
scripts build FFmpeg and mpv as shared libraries with GPL/nonfree options disabled.
The macOS playback sources include the upstream media-kit builder at v0.6.0, its
`downloads.lock`, patches, Meson wraps, and the sources referenced by that lock.
Qt's source modules are unmodified. Qt's own configure/build documentation is
included with its sources. The macOS native WebView metadata adjustment is in
`scripts/install_desktop.py`; it selects WKWebView instead of optional Chromium.
Windows makes the equivalent WebView2 selection in
`scripts/release/finalize_windows_package.ps1`. Microsoft release CRT DLLs are
installed app-locally for per-user installations; Neden updates must carry their
security updates. Linux stages the Chromium helper and resources explicitly.

## Replacing libraries

Keep a backup of the application before replacing any library. Replacement builds
need a compatible architecture, ABI, and Qt plugin version. Rebuilding the whole
application against modified libraries is also supported by the MIT source.

- **macOS:** frameworks live in `Neden.app/Contents/Frameworks`, Qt plugins in
  `Contents/PlugIns`, and QML imports in `Contents/Resources/qml`. Replacing a binary
  invalidates its signature. Sign modified nested libraries/plugins and then the
  outer app with your own identity, or ad hoc with `codesign --force --sign -`.
  The installer script documents the signing order and verifies all linkage.
- **Windows:** DLLs live beside `bin/Neden.exe`; Qt plugins and QML imports are
  located by `bin/qt.conf`. Quit Neden before replacing a DLL. MSVC is used for the
  application and Qt; the playback runtime uses the C ABI from MSYS2 UCRT64.
- **Linux:** the private application tree is under `/opt/neden`. The executable's
  relative runtime paths and `qt.conf` locate its libraries. Libraries supplied
  by the distribution can be rebuilt/replaced through its package manager.

The downloadable candidates are not a claim of notarization, SmartScreen
reputation, or hardware certification. Published release notes state the actual
platforms and checks used. Publication requires the complete dependency inventory
and source/notices review, not just a successful compile.
