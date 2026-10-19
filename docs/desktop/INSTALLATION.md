# Installing and updating Neden on this Mac

From `~/projects/neden`, run:

```sh
bun run install:desktop
```

Use this after desktop source changes so the version opened from Finder, Spotlight, and the Dock actually includes the work. A successful `build:desktop` alone does not update the installed app. The Codex **Update installed Neden** action runs the same installer.

The installer builds Release, runs desktop tests and QML lint, copies the reviewed playback frameworks, deploys Qt/QML/native plugins (including the SecureTransport HTTPS backend and SVG image plugin), removes local development library search paths, and rejects unbundled dependencies. It signs the nested code and main bundle with the existing persistent Apple Development identity, checks signatures and playback licensing configuration, and installs `/Applications/Neden.app`. It gracefully quits an existing installed copy first so the latest position is saved; it retains the old bundle until replacement succeeds and restores it if installation/launch fails. It never installs or changes the IINA plugin.

Use `python3 scripts/install_desktop.py --no-launch` when the user is using that Mac and the installed app should remain closed.

The app, Finder display name, menu, and executable are **Neden**. Preserve bundle ID `io.github.hanifcarroll.neden.qt-lab` and the existing certificate so routine rebuilds retain Keychain approvals. Do not substitute ad hoc signing for the main app. If the certificate or Keychain permissions change, macOS may require approval; do not automate protected system dialogs.

To verify an update, inspect `NedenSourceRevision` in `/Applications/Neden.app/Contents/Info.plist`, confirm the running executable is `/Applications/Neden.app/Contents/MacOS/Neden`, and run `codesign --verify --deep --strict /Applications/Neden.app`. The source-revision field includes `-dirty` if built from uncommitted work. The installed bundle contains its libraries; it does not need the repository's build folder at runtime.

## Normal use

Open Neden and choose **Open video** (Command–O). Choose **Subtitles → Load primary track…** and **Load secondary track…** to load two independent UTF-8 SRT files. Both appear together over the video; both tracks are selectable, and the primary track is searchable. Track choices are session-only and do not imply any fixed language pair.

On launch, **Recently played** shows up to 200 videos, each with its last position and a Resume button. Resuming starts at the precise saved timestamp, subject to video-frame/decoder accuracy. Positions are saved on normal close/quit, before switching videos, and every five seconds for recovery after an abnormal exit. **Remove** forgets the entry without deleting the file, and an actively playing removed video stays forgotten until you explicitly open it again. Unavailable files remain removable.

Space toggles play/pause; Left/Right seek five seconds; F11 and Mac Fn/Globe–F toggle fullscreen; Escape dismisses the active panel or exits fullscreen. Command–comma opens Settings, Command–W closes the window, and Command–Q quits. Use Ctrl for the equivalent Open/Settings/Close shortcuts on other platforms. Native macOS fullscreen remains optional in Settings. Command/Ctrl–Left and –Right move to the previous/next subtitle and pause; Command/Ctrl–R replays the current subtitle once, then pauses at its end. These three controls also appear beside Play in the bottom bar. Playback includes a volume slider; mute restores its previous level. File, Playback, Subtitles, Study, and View menus hold secondary actions. Double-click a word or finish selecting a phrase to pause and request an explanation in the Meaning tab. The titlebar buttons toggle the transcript and open Meaning; the tabs switch between them. The transcript follows the active cue until you scroll manually. Its search icon reveals search and language controls. Settings → Subtitles customizes both subtitle sizes, margins, text color, outline, weight, and optional compact background. See [the design system](DESIGN_SYSTEM.md) before changing UI components.

Local history and study data live under `~/Library/Application Support/Neden Qt Lab/` to retain the existing data location. `recent-videos.json` contains video paths, positions, and durations only; no subtitle choices, language presets, model output, or credentials. Malformed history is preserved and reported instead of being overwritten.

## Isolated native checks

Use an empty temporary directory with `open -n /Applications/Neden.app --args --profile /absolute/test-directory`. This isolates history and study settings from the user's data. Use local media and controlled/mock services; do not trigger live AI or subtitle APIs without authorization.

Exercise Open, both subtitle pickers and simultaneous text, seeking, Close/Quit, reopening from the recent list, switching between two videos, and removal while playing followed by another restart. Confirm the normal Mac menu/settings/fullscreen controls. Pure tests and QML lint do not establish visible native behavior. Close the isolated instance before reopening the normal installed app.

In a separate build directory such as `build/desktop-ui`, the opt-in `NEDEN_NATIVE_UI_CHECKS=ON` CMake target, `NedenNativeChecks`, exercises the real QML window and libmpv on a visible Mac display. Supply `NEDEN_CHECK_VIDEO`, `NEDEN_CHECK_PRIMARY`, and `NEDEN_CHECK_SECONDARY` as absolute paths to local media/SRT files. It uses a temporary profile and no saved credentials. It also checks the bundled HTTPS backend against an unauthenticated API endpoint (no paid request), and uses a local synthetic streaming server to exercise word selection and explanation rendering. To avoid requiring Accessibility privileges, these checks use Qt's file-picker implementation and deliver its selected-file signal; they do not automate Apple's native Open panel. They verify that both subtitle text items are visible, Settings opens, a window/services restart restores the precise position without retaining subtitle choices, and removing a playing entry remains effective after another restart. Keep these interaction checks on the MacBook Pro when the Air is in use.

Keep the normal install/build directory configured with native UI checks disabled. The installer enforces this so Qt's lint tooling does not combine the same QML URI from two executables. Build the opt-in runner separately when needed.
