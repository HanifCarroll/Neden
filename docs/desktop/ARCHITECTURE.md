# Desktop architecture

`apps/desktop` contains a single Qt Quick application built by CMake. QML views are in `ui/`; application services and native adapters are small C++/Objective-C++ files beside the build definition. Qt Test targets exercise parsing, Turkish and Unicode search, session ownership, study services, safe explanation formatting, render lifetime, and video geometry.

- `LearningSession` owns subtitle selection, current/translated cues, replay, and playback interaction. `CueModel` parses and searches subtitles without changing their displayed text.
- `PlaybackEngine` isolates session logic from libmpv. `MpvPlayback` owns the reviewed player, bounded buffers, queued events, and pending media requests. `Video` creates its GPU context only after media is requested; renderer ownership tokens reject stale pending loads.
- `Video` renders into a Qt Quick OpenGL color texture. It preserves the sRGB declaration and nonblocking mpv timing fix. The idle texture is 64 × 64 with no depth/stencil attachment. Qt uses native text rendering.
- `StudyTools` owns settings, streamed requests, subtitle search/download, local cards, review, and export. `ExplanationFormat` uses Qt's Markdown parser while making links inert and replacing images with text. No model HTML is executed.
- `credentials_mac.mm` reads existing native Keychain items; credentials do not enter QML state. Other platforms fail closed until their credential-store adapters are verified.
- `FullscreenController` bridges public AppKit APIs for instant borderless or optional native fullscreen. Other platforms use the window manager's fullscreen mechanism.

The dictionary WebView exists only while open. Settings and study panels are constructed on first use, then retain drafts/state. There is no sample-media autoload, comparison driver, or periodic profiling timer.

`RecentVideos` is a bounded local JSON history model with atomic writes. It stores only video paths and precise playback positions/durations. `LearningSession` checkpoints on video switches, normal quit/close, and every five seconds, and ignores a removed active video until explicitly opened again. Subtitle choices remain session-only. Resume seeks use the full stored precision and reject another video's stale load callback. Sentence replay preserves the original viewing position for history. `mediaPathIdentity` resolves aliases and normalizes Mac paths to NFC so Finder and file-picker Unicode spellings identify the same video; it does not change subtitle text.

The app/executable is named **Neden**. Retain `io.github.hanifcarroll.neden.qt-lab` as the bundle/signing identifier. Existing study data and history use the established `Neden Qt Lab` application-support directory explicitly; this legacy directory name does not appear in the product UI. `DesktopApplication` handles Finder Open With events without taking over default video associations.
