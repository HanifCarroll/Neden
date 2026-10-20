# Installing and updating Neden on this Mac

From `~/projects/neden`, run:

```sh
bun run install:desktop
```

Use this after desktop source changes so the version opened from Finder, Spotlight, and the Dock actually includes the work. A successful `build:desktop` alone does not update the installed app. The Codex **Update installed Neden** action runs the same installer.

The installer builds Release, runs desktop tests and QML lint, copies the reviewed playback frameworks, deploys Qt/QML/native plugins (including the SecureTransport HTTPS backend and SVG image plugin), removes local development library search paths, and rejects unbundled dependencies. It signs the nested code and main bundle with the existing persistent Apple Development identity, checks signatures and playback licensing configuration, and installs `/Applications/Neden.app`. It gracefully quits an existing installed copy first so the latest position is saved; it retains the old bundle until replacement succeeds and restores it if installation/launch fails. It never installs or changes the IINA plugin.

Use `python3 scripts/install_desktop.py --no-launch` when the installed app should remain closed after an authorized update. This still quits and replaces the running app. If installation is explicitly deferred because the user is watching, build and test the source only; do not run either installer command.

The app, Finder display name, menu, and executable are **Neden**. Preserve bundle ID `io.github.hanifcarroll.neden.qt-lab` and the existing certificate so routine rebuilds retain Keychain approvals. Do not substitute ad hoc signing for the main app. If the certificate or Keychain permissions change, macOS may require approval; do not automate protected system dialogs.

To verify an update, inspect `NedenSourceRevision` in `/Applications/Neden.app/Contents/Info.plist`, confirm the running executable is `/Applications/Neden.app/Contents/MacOS/Neden`, and run `codesign --verify --deep --strict /Applications/Neden.app`. The source-revision field includes `-dirty` if built from uncommitted work. The installed bundle contains its libraries; it does not need the repository's build folder at runtime.

## Normal use

Open Neden and choose **Open video** (Command–O). Use the **Primary…** and **Secondary…** buttons above the transcript, or **Subtitles → Load primary track…** and **Load secondary track…** to load two independent UTF-8 SRT or WebVTT files. Both appear together over the video; both tracks are selectable, and the primary track is searchable. Each video remembers its own primary and secondary files and reloads them in the same roles when resumed. No fixed language pair is imposed. If a file has moved or its drive is disconnected, the other track and video still load; reconnect the drive or choose a replacement. The saved association is retained until replaced.

On launch, **Recently played** shows up to 200 videos, as compact clickable rows with their last positions. Click a row to resume. Resuming starts at the precise saved timestamp, subject to video-frame/decoder accuracy. Positions are saved on normal close/quit, before switching videos, and every five seconds for recovery after an abnormal exit. **Remove** forgets the entry without deleting the file, and an actively playing removed video stays forgotten until you explicitly open it again. Unavailable files remain removable.

Double-click the video to enter or leave fullscreen; double-clicking subtitle text still selects a word for explanation. Space toggles play/pause; Left/Right seek five seconds; F11 and Mac Fn/Globe–F toggle fullscreen; Escape dismisses the active panel or exits fullscreen. Command–comma opens Settings, Command–W closes the window, and Command–Q quits. Use Ctrl for the equivalent Open/Settings/Close shortcuts on other platforms. Native macOS fullscreen remains optional in Settings. Command/Ctrl–Left and –Right play the previous/next subtitle once, then pause; Command/Ctrl–R replays the current subtitle once, then pauses at its end. These three controls also appear beside Play in the bottom bar. Playback includes a volume slider; M toggles mute and restores the previous level. Space continues to control playback after changing speed. File, Playback, Subtitles, Study, and View menus hold secondary actions. Double-click a word or finish selecting a phrase to pause and request an explanation, including a natural translation, in the Meaning tab. Its phrase and answer stay stable while dragging a new highlight and update only after the selection is committed. The video reaches the top edge without an episode-title strip. Sidebar tabs at the top switch between Transcript and Meaning; Close hides the sidebar, and the captions button in the playback bar reopens it. Native Mac window buttons remain over the picture. The transcript follows the active cue until you scroll manually. Its search icon reveals search and language controls. Settings → Subtitles customizes both subtitle sizes, margins, text color, outline, weight, and optional compact background. See [the design system](DESIGN_SYSTEM.md) before changing UI components.

Settings → Services uses a single Save settings action for the complete draft; changing fields does not activate them until the save succeeds. Provider choices use named IDs, and credentials remain separate from the draft.

The Meaning tab supports follow-up questions, Stop, Retry, and replay of the explained cue. Saved flashcards and replay always use the original explained cue and its timestamp, even after you seek elsewhere. Enter sends a question; Shift–Enter adds a line. See [plugin behavior parity](PLUGIN_PARITY.md) for context ownership, optional word meanings, provider settings, and appearance previews.

Local history and study data live under `~/Library/Application Support/Neden Qt Lab/` to retain the existing data location. `recent-videos.json` version 2 contains video paths, positions, durations, and optional primary/secondary subtitle file paths; no subtitle contents, language presets, model output, or credentials. Existing version 1 history remains readable. OpenSubtitles downloads are saved as bounded, content-addressed SRT files under `subtitles/` in the same application-support directory, so resuming uses the saved file without another download. Malformed history is preserved and reported instead of being overwritten.

## Isolated native checks

Use an empty temporary directory with `open -n /Applications/Neden.app --args --profile /absolute/test-directory`. This isolates history and study settings from the user's data. Use local media and controlled/mock services; do not trigger live AI or subtitle APIs without authorization.

Exercise Open, both subtitle pickers and simultaneous text, seeking, Close/Quit, reopening from the recent list, switching between two videos, and removal while playing followed by another restart. Confirm the normal Mac menu/settings/fullscreen controls. Pure tests and QML lint do not establish visible native behavior. Close the isolated instance before reopening the normal installed app.

In a separate build directory such as `build/desktop-ui`, the opt-in `NEDEN_NATIVE_UI_CHECKS=ON` CMake target, `NedenNativeChecks`, exercises the real QML window and libmpv on a visible Mac display. Supply `NEDEN_CHECK_VIDEO`, `NEDEN_CHECK_PRIMARY`, and `NEDEN_CHECK_SECONDARY` as absolute paths to local media/SRT files. It uses a temporary profile and no saved credentials. It also checks the bundled HTTPS backend against an unauthenticated API endpoint (no paid request), and uses a local synthetic streaming server to exercise word selection and explanation rendering. To avoid requiring Accessibility privileges, these checks use Qt's file-picker implementation and deliver its selected-file signal; they do not automate Apple's native Open panel. They verify that both subtitle text items are visible, Settings opens, a window/services restart restores the precise position and both chosen subtitle tracks in their original roles, and removing a playing entry remains effective after another restart. Keep these interaction checks on the MacBook Pro when the Air is in use.

Keep the normal install/build directory configured with native UI checks disabled. The installer enforces this so Qt's lint tooling does not combine the same QML URI from two executables. Build the opt-in runner separately when needed.

### Off-screen subtitle interaction checks

For caption hover and source-card regressions without opening windows or disturbing another application, use the separately built `NedenNativeChecks` target:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software NEDEN_CHECK_HEADLESS=1 \
  build/desktop-ui/NedenNativeChecks \
  explainHoverCoversBothTracksAndThePathToTheButton \
  explainHoverSurvivesActiveSelections \
  explanationPhraseUsesThePluginCardStyle \
  hoveringAndExplainingTheFullLineUseOneExplicitRequest \
  singleClickShowsContextualWordCardWithoutChangingTheSidebar \
  videoAndSidebarReachTheTopWithoutATitleStrip \
  trackDropdownsDisplayTheirNamedRolesAndReadableSelection \
  settingsDraftAndShortcutPolicyUseTheActualUi \
  streamingRepliesStayAnchoredToTheLatestQuestion
```

This uses the actual QML with software rendering, a temporary profile and placeholder media, and a local mock AI endpoint. It checks both tracks, subtitle order, wrapping, resizing, pointer movement to the button, phrase-card styling, single-click contextual cards, double-click request separation, edge-to-edge video/sidebar placement, explicit request submission and follow-up editing. Caption-action checks include held mouse presses, drag/reentry, selections on either/both tracks, sidebar modes, answer/input focus, word-card coexistence, secondary-only cues, and disabled text selection. Controlled streaming checks cover an initial answer, manual scrolling during a stream, follow-up questions, re-explaining the same phrase, retries, and hidden model names. Native-only checks are skipped in this mode. It does not verify video rendering, macOS window chrome, or fullscreen animation; keep those checks on the Pro’s real display.
