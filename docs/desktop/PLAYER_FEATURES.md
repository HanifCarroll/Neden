# Media player features and validation

The media player work is divided into decoding, durable choices, queue progression, subtitle timing, operating-system integration, and presentation. No installed application is updated by the build or tests. Interactive validation uses an isolated profile on the MacBook Pro.

## Module contracts

- `PlaybackEngine` exposes typed commands and observable tracks, chapters, devices, capabilities and playback state. Only `MpvPlayback` speaks the mpv protocol. Commands are asynchronous; final seeks are exact, scrub previews are coalesced. External subtitle requests match their file and role generation so late completions cannot replace a newer choice.
- `PlayerPreferences` owns global volume/output/encoding/speed preferences and per-media audio, sync and picture choices. Network URLs are not persisted. Disabling history disables per-media writes; clearing history also removes per-media preferences.
- `MediaQueue` expands folders, sorts filenames naturally, deduplicates canonical paths, and owns progression/reordering. Autoplay is explicitly opt-in and belongs to the current queue. Removing a queue entry does not delete its file or stop the currently playing video.
- `SubtitleTimeline` owns independently shifted primary and secondary text timelines. Original timestamps are retained, so repeated delay changes do not accumulate. Decoding accepts valid Unicode or an explicit legacy encoding, including Windows-1254 and ISO-8859-9 for Turkish.
- `ThumbnailService` has one lazy worker, one replaceable request and one preview image. Its decoder runs off the GUI thread and is released after ten idle seconds. It handles local files only; hovering network media shows the time without launching a second network request.
- `PlatformMediaSession` owns system transport registration and display-sleep inhibition, and releases them on teardown. macOS uses MediaPlayer/IOKit/CoreAudio, Linux uses MPRIS and the desktop Inhibit portal, Windows uses SMTC and execution-state requests. macOS pauses when the default audio route changes; the mpv device-list callback pauses on device removal on other platforms.
- `LearningSession` coordinates these modules with existing study interactions and precise recent-history checkpoints. QML invokes these interfaces rather than assembling backend commands.

## User-visible operations

| Area | Implemented entry points |
| --- | --- |
| Open and queue | File menu: files, folder, direct HTTP/HTTPS URL, append files; drop video/subtitle files; Playback → Queue for play, reorder, remove, clear and optional automatic advance |
| Navigation | Previous/next video, chapters, jump to time, five-second seeking, coalesced scrubbing with exact release, undo last seek, explicit replay/start-over, frame stepping and A–B loop |
| Audio | Track selection with title/language/codec/default/forced labels, output selection, per-video audio delay, volume/mute, night-mode compression, speed choices and 0.05× increments/reset |
| Subtitles | External SRT/VTT, explicit legacy text decoding, embedded/native tracks, ASS/SSA and bitmap files through mpv, separate text offsets, remembered roles and delays |
| Picture | Rotation, aspect override, fit/fill, deinterlace and reset; hardware decoder and media information |
| Window | Instant/fullscreen preferences retained, floating always-on-top mode, compact controls for small windows, fullscreen control/cursor inactivity hiding |
| Capture | Clean source frame from mpv with asynchronous Qt PNG encoding, or displayed video with both QML subtitle tracks via the video-stage capture; success/failure feedback |
| History | Resume, remove, locate a missing file, clear all history, disable future history, explicit speed persistence |
| System | Screen-awake lifecycle, system transport/Now Playing adapters, pause on audio-device removal |

Standalone arrow/M shortcuts remain suppressed while editing text or operating a focused control. Command/Ctrl subtitle navigation keeps its play-once-and-pause behavior. A single click on a paused video with the sidebar open closes the sidebar and resumes; the delayed single-click handler preserves double-click fullscreen and subtitle text interaction.

The Explain line action activates only on actual word geometry, including when text is selected. It remains briefly reachable across the gap to its button, and button focus does not keep it visible over empty video space. Explanation prompts lead with natural translation and teach spoken vocabulary/grammar using subtitle labels only as context.

## Runtime and platform limits

- The reviewed macOS runtime is mpv 0.36.0. It lacks `secondary-sub-delay`. SRT/VTT timing is independent in Neden's own timelines; two native subtitle tracks share the runtime's single delay. Capability detection avoids unsupported-property errors and the subtitle controls describe this distinction. A newer audited playback runtime is needed for independent timing of two native tracks.
- Native embedded/styled/bitmap subtitles are rendered by mpv. They do not provide the selectable word overlays/transcript of imported SRT/VTT. Primary ASS styling is preserved; secondary styling and bitmap placement follow mpv limitations. Two bitmap tracks can overlap.
- Precise seeks use a 0.1-second demuxer preroll to preserve reference frames when refreshing a paused native subtitle selection. The final target stays exact; decoding from an earlier keyframe can add seek work. This uses mpv's documented [demuxer seek-offset workaround](https://mpv.io/manual/stable/#options-hr-seek-demuxer-offset).
- Direct URLs mean playable media URLs; website extraction, DRM, casting and disc menus are not implemented. Network URLs remain in the in-memory queue only.
- Floating mode is an always-on-top player window, not Apple's native Picture in Picture service.
- Screenshots with QML subtitles capture the displayed stage resolution and letterboxing. Clean screenshots copy mpv's source frame into owned pixels and encode PNG with Qt off the UI thread. Atomic writes preserve an existing destination on failure; repeated requests are bounded to one capture at a time. HDR export and color fidelity require dedicated fixtures and inspection.
- Windows still needs a reviewed playback runtime and credential adapter; the full build remains intentionally gated. Linux needs the reviewed runtime recipe and credential integration. Neither platform's physical playback or OS controls are established by a macOS build.

## Reference implementations

These are behavior/API/architecture references. No GPL player source was copied or translated into Neden.

- [mpv manual](https://mpv.io/manual/stable/) and [client API](https://github.com/mpv-player/mpv/blob/master/include/mpv/client.h): asynchronous commands, track/chapter/device metadata, native subtitles, seeking, screenshots, filters and pitch-preserving speed changes. The adapter tests inspect actual bundled capabilities rather than assuming the latest manual matches the runtime.
- [mpv select.lua](https://github.com/mpv-player/mpv/blob/master/player/lua/select.lua): track labels and chapter navigation behavior.
- [thumbfast](https://github.com/po5/thumbfast/blob/master/thumbfast.lua), MPL-2.0: lazy preview generation, replacing obsolete requests and freeing idle decoder resources; independently implemented with Qt and libmpv's software render API.
- [IINA NowPlayingInfoManager](https://github.com/iina/iina/blob/develop/iina/NowPlayingInfoManager.swift): system transport lifecycle behavior.
- [Clapper](https://github.com/Rafostar/clapper): explicit queue progression modes and separation of player state.
- [Apple MPRemoteCommandCenter](https://developer.apple.com/documentation/mediaplayer/mpremotecommandcenter), [MPRIS Player interface](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html), and [Windows SMTC desktop interop](https://learn.microsoft.com/en-us/windows/win32/api/systemmediatransportcontrolsinterop/nf-systemmediatransportcontrolsinterop-isystemmediatransportcontrolsinterop-getforwindow): platform adapter contracts.

## Validation ledger

Testing used a physical M1 Pro display on macOS 27.2 with Qt 6.12, temporary profiles, the reviewed mpv 0.36 runtime, Leyla ile Mecnun episode 039 with Turkish/English sidecars, and short synthetic multi-track/chapter fixtures. Neither the Air's installed app nor the Pro's existing checkout/application was replaced. No saved credentials or paid AI requests were used.

- Release builds, nine headless CTest suites, and QML lint passed. New cases cover natural queue ordering/removal, opt-in EOF progression, mixed-drop subtitle loading, scrub undo, durable preferences, Turkish decoding, independent text offsets, deduplicated subtitle discovery, bundled runtime capabilities, and screenshot pixel conversion/bounds.
- At `b930b60`, the native interaction suite passed 20 checks; one remaining assertion used the old speed-menu row number. The corrected active-row assertion passed at `456b1f2`. Hover/selection, word cards, dual subtitle restoration, readable dropdowns, keyboard focus, fullscreen toggling and chat scroll anchoring passed.
- At `b930b60`, eight of nine added native feature checks passed: audio/subtitle timing, native ASS rendering, queue reorder/EOF progression, jump/scrub/undo/thumbnails, picture controls, clean screenshot encoding, captioned capture/floating mode, and macOS power/Now Playing lifecycle. Clean PNG and ASS output were visually inspected.
- At `0395f16`, targeted native regressions passed for paused-canvas sidebar close/resume, fullscreen inactivity hiding and pointer-movement reveal despite speed-control keyboard focus, paused embedded ASS selection displaying the already-active cue, and pending external ASS loading followed immediately by a seek. Native subtitle switching kept position at two seconds with zero pause-state changes. Visual review then found corrupted video pixels underneath the correctly rendered ASS cue. The synthetic MPEG-4 file decoded cleanly with standalone FFmpeg; the bundled runtime corrupted only the subtitle-refresh seek. Disabling hardware decoding, seek frame dropping, or decoder threading did not fix it. A 0.1-second demuxer preroll preserved the video pixels without changing the final timestamp. The retained regression now checks the underlying frame as well as cue appearance.
- At `5a4c51f`, the final repository-native runner passed both strengthened regressions: paused ASS selection preserved the underlying video and current position, and paused-canvas resume/fullscreen hide/reveal passed. The runner was rebuilt without the external probe source override. Earlier feature results remain tied to their recorded revisions.
- The Linux DBus translation unit passed an isolated compile against Qt DBus on macOS. That is a source check, not a Linux runtime test. Windows code remains uncompiled here.

Evidence and the independent feature harness are retained in the shared iCloud folder `Neden Testing/2026-10-20`; each log names its tested revision. These are native Qt harness results on a real display, not a manual end-user acceptance test: the computer-use tool could not attach to Neden's accessibility interface. System metadata and sleep assertions were checked; physical media-key dispatch, audio-device unplugging, HDR/SDR fidelity, multichannel audio, multi-display/refresh-rate behavior, Windows, and Linux still need separate validation.
