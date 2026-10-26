# Installing and updating Neden on macOS

Run these commands from any checkout directory. Follow the [README setup steps](../../README.md#build-the-desktop-player-on-macos) first.

```sh
python3 scripts/install_desktop.py --no-launch
```

The installer builds Release, runs desktop tests and QML lint, stages the reviewed playback frameworks and Qt plugins, removes development library search paths, verifies signatures and playback configuration, then replaces `/Applications/Neden.app`. It gracefully quits an existing installed copy to save its position, and retains a backup until replacement succeeds. It does not change IINA or its plugin. Close playback before installing an update.

Omit `--no-launch` to open the installed app after a successful update. To install without administrator access, use a directory you own:

```sh
python3 scripts/install_desktop.py --no-launch --destination ~/Applications/Neden.app
```

Fresh builds use ad hoc signing and need no Apple developer account. macOS may request Keychain access again after rebuilding an ad hoc application. If you have a persistent development certificate, configure its name explicitly:

```sh
NEDEN_SIGNING_IDENTITY='Apple Development: Your Name (TEAMID)' sh scripts/desktop.sh build
```

The choice remains in the local CMake cache. The installer refuses to replace a certificate-signed installation with an ad hoc build; use the same certificate or a separate destination. It never automates Keychain permission dialogs. The stable bundle identifier `io.github.hanifcarroll.neden.qt-lab` preserves existing data and credential associations; it does not require access to the maintainer's account.

Verify an update with `codesign --verify --deep --strict /Applications/Neden.app` and the `NedenSourceRevision` field in `Contents/Info.plist`. The field includes `-dirty` for uncommitted work. The installed bundle contains its runtime libraries and does not need the checkout at runtime. This is local development packaging; see [distribution requirements](DEPENDENCIES.md) before sharing binaries.

## Normal use

Open Neden and choose **Open video** (Command–O). Use the **Primary** and **Secondary** track pickers above the transcript (choose **Load subtitle file…**), or **Subtitles → Load primary track…** and **Load secondary track…** to load two independent UTF-8 SRT or WebVTT files. Both appear together over the video; both tracks are selectable, and the primary track is searchable. Each video remembers its own primary and secondary files and reloads them in the same roles when resumed. No fixed language pair is imposed. If a file has moved or its drive is disconnected, the other track and video still load; reconnect the drive or choose a replacement. The saved association is retained until replaced.

On launch, **Continue watching** shows unfinished videos. **History** includes finished videos as well; up to 200 entries are retained as compact clickable rows with their last positions. Click a row to resume. Resuming starts at the precise saved timestamp, subject to video-frame/decoder accuracy. Positions are saved on normal close/quit, before switching videos, and every five seconds for recovery after an abnormal exit. **Remove** forgets the entry without deleting the file, and an actively playing removed video stays forgotten until you explicitly open it again. Unavailable files remain removable.

Double-click the video to enter or leave fullscreen; double-clicking subtitle text still selects a word for explanation. Space toggles play/pause; Left/Right seek five seconds; F toggles instant fullscreen on the current desktop; Mac Fn/Globe–F and Control–Command–F toggle native fullscreen on its own Space. F11 follows the fullscreen preference; Escape dismisses the active panel or exits fullscreen. Command–comma opens Settings, Command–W closes the window, and Command–Q quits. Use Ctrl for the equivalent Open/Settings/Close shortcuts on other platforms. Settings chooses the fullscreen mode used by the toolbar, double-click gesture, and green window button; the F and Fn–F shortcuts keep their dedicated modes. Command/Ctrl–Left and –Right play the previous/next subtitle once, then pause; Command/Ctrl–R replays the current subtitle once, then pauses at its end. These three controls also appear beside Play in the bottom bar. Playback includes a volume slider; M toggles mute and restores the previous level. Space continues to control playback after changing speed. File, Playback, Subtitles, Study, and View menus hold secondary actions. Double-click a word or finish selecting a phrase to pause and request an explanation, including a natural translation, in the Explanation tab. Its phrase and answer stay stable while dragging a new highlight and update only after the selection is committed. The video reaches the top edge without an episode-title strip. Sidebar tabs at the top switch between Transcript and Explanation; Close hides the sidebar, and the captions button in the playback bar reopens it. Native Mac window buttons remain over the picture. The transcript follows the active cue until you scroll manually. Its search icon reveals the search field; case matching follows the chosen subtitle language. Settings → Subtitles customizes both subtitle sizes, margins, text color, outline, weight, and optional compact background. See [the design system](DESIGN_SYSTEM.md) before changing UI components.

Settings → AI & accounts shows Neden Cloud or your own API provider, according to the selected connection mode. Switching the mode takes effect immediately; it preserves your personal provider configuration. Cloud sign-in, account management and saved-account connection are separate from API credentials. Languages/context, subtitle downloads and advanced provider options expand when needed. Save settings applies the complete configuration draft; keys are saved separately in the system credential store. Subtitle appearance also has expandable advanced controls, with explicit Apply and preview cancellation.

The Explanation tab supports follow-up questions, Stop, Retry, and replay of the explained cue. Saved flashcards and replay always use the original explained cue and its timestamp, even after you seek elsewhere. Enter sends a question; Shift–Enter adds a line. See [plugin behavior parity](PLUGIN_PARITY.md) for context ownership, optional word meanings, provider settings, and appearance previews.

Settings → Shortcuts contains the editable keyboard map. Command–B toggles the sidebar; E pauses and explains the current subtitle. Overrides are saved separately in `shortcuts.json`; standard editing/window shortcuts and macOS native fullscreen remain fixed.

Local history and study data live under `~/Library/Application Support/Neden Qt Lab/` to retain the existing data location. `recent-videos.json` version 3 contains video paths, positions, durations, optional primary/secondary subtitle file paths, native track IDs and timing offsets; no subtitle contents, language presets, model output, or credentials. Existing version 1 and 2 history remains readable. OpenSubtitles downloads are saved as bounded, content-addressed SRT files under `subtitles/` in the same application-support directory, so resuming uses the saved file without another download. Malformed history is preserved and reported instead of being overwritten.

See [media player features and runtime limits](PLAYER_FEATURES.md) for queue, audio/picture controls, native subtitle formats, screenshots, timeline previews and platform integration.

## Isolated native checks

Use an empty temporary directory with `open -n /Applications/Neden.app --args --profile /absolute/test-directory`. This isolates history and study settings from the user's data. Use local media and controlled/mock services; do not trigger live AI or subtitle APIs without authorization.

Exercise Open, both subtitle pickers and simultaneous text, seeking, Close/Quit, reopening from the recent list, switching between two videos, and removal while playing followed by another restart. Confirm the normal Mac menu/settings/fullscreen controls. Pure tests and QML lint do not establish visible native behavior. Close the isolated instance before reopening the normal installed app.

In a separate build directory such as `build/desktop-ui`, the opt-in `NEDEN_NATIVE_UI_CHECKS=ON` CMake target, `NedenNativeChecks`, exercises the real QML window and libmpv on a visible Mac display. Supply `NEDEN_CHECK_VIDEO`, `NEDEN_CHECK_PRIMARY`, and `NEDEN_CHECK_SECONDARY` as absolute paths to local media/SRT files. It uses a temporary profile and no saved credentials. It also checks the bundled HTTPS backend against an unauthenticated API endpoint (no paid request), and uses a local synthetic streaming server to exercise word selection and explanation rendering. To avoid requiring Accessibility privileges, these checks use Qt's file-picker implementation and deliver its selected-file signal; they do not automate Apple's native Open panel. They verify that both subtitle text items are visible, Settings opens, a window/services restart restores the precise position and both chosen subtitle tracks in their original roles, and removing a playing entry remains effective after another restart. Use a dedicated display; these checks open windows and interact with them.

Keep the normal install/build directory configured with native UI checks disabled. The installer enforces this so Qt's lint tooling does not combine the same QML URI from two executables. Build the opt-in runner separately when needed.

The optional paused native-subtitle regression uses a static 12-second video with two ASS tracks. Generate it with a development FFmpeg executable (not the bundled playback library):

```sh
ffmpeg -f lavfi -i 'color=c=black:s=640x360:r=24:d=12' \
  -i apps/desktop/tests/fixtures/paused-track.ass \
  -map 0:v -map 1:s -map 1:s -c:v mpeg4 -c:s ass \
  -disposition:s 0 /tmp/neden-paused-ass.mkv
```

Set `NEDEN_CHECK_ASS_VIDEO=/tmp/neden-paused-ass.mkv` when running `NedenNativeChecks`. The regression selects subtitle ID 2 while paused at two seconds and requires the current cue and changed video pixels without a test-issued seek, pause-state change, or position drift. It skips explicitly when the fixture is not supplied.

### Off-screen subtitle interaction checks

For caption hover and source-card regressions without opening windows or disturbing another application, use the separately built `NedenNativeChecks` target:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software NEDEN_CHECK_HEADLESS=1 \
  build/desktop-ui/NedenNativeChecks \
  explainHoverCoversBothTracksAndThePathToTheButton \
  explainHoverSurvivesActiveSelections \
  explainButtonFootprintAndStationaryCueChanges \
  pausedVideoHidesControlsAndCursorUntilPointerMoves \
  explanationPhraseUsesThePluginCardStyle \
  hoveringAndExplainingTheFullLineUseOneExplicitRequest \
  singleClickShowsContextualWordCardWithoutChangingTheSidebar \
  videoAndSidebarReachTheTopWithoutATitleStrip \
  trackDropdownsDisplayTheirNamedRolesAndReadableSelection \
  settingsDraftAndShortcutPolicyUseTheActualUi \
  streamingRepliesStayAnchoredToTheLatestQuestion
```

This uses the actual QML with software rendering, a temporary profile and placeholder media, and a local mock AI endpoint. It checks both tracks, subtitle order, wrapping, resizing, pointer movement to the button, phrase-card styling, single-click contextual cards, double-click request separation, edge-to-edge video/sidebar placement, explicit request submission and follow-up editing. Caption-action checks include held mouse presses, drag/reentry, selections on either/both tracks, sidebar modes, answer/input focus, word-card coexistence, secondary-only cues, and disabled text selection. Controlled streaming checks cover an initial answer, manual scrolling during a stream, follow-up questions, re-explaining the same phrase, retries, and hidden model names. Native-only checks are skipped in this mode. It does not verify video rendering, macOS window chrome, or fullscreen animation; run those checks on a real display.

### Language preferences

Settings → AI & accounts → Languages and context offers language lists for **Subtitle language** and **Explain in**. New profiles detect the language from the subtitle text and use the system language for explanations. Choose a specific subtitle language when enabling automatic OpenSubtitles downloads. Online search has its own language picker for any provider-supported language; it does not impose a primary/secondary language pair. Recognized preferences saved by older versions are retained; unrecognized free-text values fall back individually to these defaults. Language choices do not guarantee a subtitle match or equal AI quality for every language.
