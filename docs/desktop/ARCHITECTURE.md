# Desktop architecture

The Qt Quick desktop application is organized around complete workflows. QML renders state and submits user actions; native modules own context, cancellation, persistence, and playback ownership. The IINA plugin remains independent.

```text
apps/desktop/
  app/        composition, settings, profile storage, history, player session
  playback/   libmpv adapter, video surface, geometry and renderer lifetime
  subtitles/  file decoding, two-track timeline, transcript, subtitle acquisition
  study/      frozen targets, study interactions, conversations, AI client, word cards, deck
  platform/   native credentials and fullscreen implementations
  tests/      portable tests and opt-in native/off-screen interaction checks
  ui/         QML views, design tokens, controls and keyboard policy
  macos/      bundle metadata and icon
```

## Module contracts

- `LearningSession` coordinates media loading, exact resume checkpoints, playback and sentence replay. It delegates subtitle data to `SubtitleTimeline` and study gestures to `StudyInteraction`. `PlaybackEngine` remains the testable boundary around libmpv.
- `SubtitleTimeline` owns the primary transcript model and independent secondary timeline. It decodes bounded UTF-8 SRT/WebVTT before replacing a track, handles role reuse/removal, resolves timed text and adjacent cues, and captures contextual selections. Transcript cue storage is private; its public entries are read-only. Search preserves displayed Unicode and the existing Turkish matching policy.
- `StudyTarget` is a value snapshot containing the selection, role, full cue, neighboring cues, translation clues, languages, local media identity, playback position and cue interval. Local media paths stay out of AI requests. Context is captured once; subsequent playback changes cannot change a displayed explanation or the card/replay derived from it.
- `StudyInteraction` owns preview versus committed selection, duplicate gesture suppression, word-card transitions, and whether closing a study interaction may resume playback. QML invokes complete commands (`commitSelection`, `explainCue`, `explainWord`); it does not sequence selection and network operations itself.
- `StudyServices` is a composition root exposing independent feature objects. It has no shared status message, forwarding API, or catch-all change signal.
- `ExplanationSession` owns conversation requests, streaming completion, explicit retries, optional five-second fallback, cancellation and the frozen explanation target. `Conversation` retains its pure history/context rules, limits and omission of partial answers from subsequent prompts.
- `WordLookup` owns bounded, plain-text word cards, structured response validation and legacy Google/Microsoft translation. `StudyInteraction` supplies its frozen target. A double-click or phrase selection does not also submit a delayed word request.
- `AiClient` owns authenticated Chat Completions request construction, endpoint/credential validation, provider options, token budgets, redirect policy and bounded network buffers. Both explanation and word requests use it; DeepSeek thinking remains disabled. Feature-specific streaming/card parsers retain their own output contracts. There is no automatic retry in the transport.
- `SubtitleService` owns video fingerprints, search, account login, download allowance requests, restricted redirects, validated SRT storage and media/request ownership. It has its own results and message. It publishes a stable saved path only after the download is committed.
- `FlashcardDeck` owns bounded cards, review scheduling, atomic updates and TSV export. Its message is independent of explanations and subtitle acquisition. `StudyInteraction` saves the explanation's original target and answer, never a mixture of that answer with the current playhead.
- `AppSettings` parses Qt form drafts into typed connection/learning settings. Provider choices expose stable IDs and names rather than positional mappings. All fields validate and persist before active settings change; failed saves leave active values unchanged. Credential actions use the native adapter and return status only.
- `ProfileStore` privately coordinates atomic updates to the existing `study.json` document. Settings writes preserve cards and card writes preserve settings. Version 1 data remains readable; damaged documents are preserved and writes are disabled. No migration, duplicate profile, or plaintext credential storage is introduced.

## Presentation and platform boundaries

`PlayerShortcuts.qml` owns playback-key eligibility. Editable input, control-key handling, and read-only subtitle selection are distinguished so Space/M do not disappear when a study view opens. `Main.qml` owns window/panel presentation; the inspector has one mode (hidden, transcript, meaning or dictionary). Dialogs and study panels remain lazy, and the dictionary WebView exists only while open.

`ExplanationFormat` keeps model links inert and replaces images with text; model HTML and remote resources are not executed. Word-card fields render as plain text. The shared `ChoiceBox` honors named model roles and keeps full subtitle filenames readable.

Subtitle text keeps the I-beam cursor. A subtle word hover uses the same Unicode word boundaries as lookup and the editor's glyph positions; it never changes selection, steals focus, or requests a definition. Hover disappears during selection and outside the text. Buttons use a pointing-hand cursor.

The rendering and platform adapters retain the established behavior: sRGB video texture, nonblocking mpv rendering, small idle framebuffer, native Mac text/window buttons, borderless fullscreen by default, optional native fullscreen, and protected Keychain access. The video and sidebar reach the top edge without an episode-title toolbar; sidebar controls respect safe areas.

The app/executable remains **Neden**, with bundle/signing identifier `io.github.hanifcarroll.neden.qt-lab`. Existing data stays in `Neden Qt Lab` under application support. `RecentVideos` retains version 2 records, independent subtitle paths, precise positions, malformed-data protection, and removal semantics. Source changes do not update the installed app until the installer runs; use the [installation guide](INSTALLATION.md).

## Verification

Portable tests exercise module behavior and controlled local HTTP/SSE contracts. Regressions cover saving/replaying an explained cue after seeking elsewhere, transaction failure, independent feature messages, Unicode/Turkish text, profile compatibility, cancellation, replay and history restoration. Off-screen checks exercise the actual QML controls, word/phrase gestures, follow-ups, provider models, draft saving and shortcut focus policy without saved keys or visible windows. They do not establish native macOS presentation or playback performance; those checks belong on the Pro's real display.
