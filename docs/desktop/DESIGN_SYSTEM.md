# Desktop design system

`apps/desktop/ui/Theme.qml` is the source of truth for colors, type sizes, spacing, and corner radii. Use its tokens when adding or changing a surface. Do not add tiny all-caps category headings, emoji, or decorative labels above titles.

- Typography: Inter, 12 px captions, 14 px body/controls, 20 px section titles, 24 px page headings. Use regular for body copy and semibold for headings.
- Spacing: 4, 8, 12, 16, 24, 32 px. Panels use 16 or 24 px padding; related controls use 8 or 12 px gaps.
- Shape: 6 px icon controls, 8 px form controls, 12 px dialogs/cards. The video and transcript meet flush, without outer cards, decorative borders, or blur effects.
- Controls: `IconButton`, `InspectorTab`, `ActionButton`, `SearchField`, `ChoiceBox`, `ControlSlider`, and `SegmentButton`. ChoiceBox explicitly sets both highlighted text and background colors; do not mix default light delegates into the dark UI.
- Native menus: File, Playback, Subtitles, Study, View. macOS uses the system menu bar; other platforms display Qt's menu bar. Keep playback, volume, and seeking near the video. Put secondary actions in menus.

`SubtitleText.qml` shares one text layout for the visible outline and selectable text, for both tracks. The default is white semibold text, a dark outline, a 28 px primary line, a 24 px secondary line, and a 36 px bottom margin. There is no background by default. Settings offers independent sizes, margin, color, outline, weight, and an optional compact background; appearance lives in `appearance.ini`, separately from recent videos. Never persist the user's subtitle track choices.

Subtitle parsing removes recognized SRT presentation tags (`i`, `b`, `u`, `s`, `font`, and line breaks) and decodes common named entities. It does not execute HTML or load resources; unknown tags remain inert literal text. This intentionally applies the user's chosen typography uniformly, instead of honoring italics or colors embedded in a subtitle file.

Selection pauses playback immediately. A completed mouse selection or Shift-key selection opens the Meaning tab and makes one explanation request. Draft highlight changes stop an in-flight response but retain its phrase and answer until the new selection is committed. The Meaning heading follows the frozen request subject, never the live highlight or changing subtitle. Switching back to Transcript restores the previous playback state. Explanations include a natural translation and breakdown; there is no separate Translate action. Explain appears only before an answer, Try again after failure/interruption, and Stop during a request. A retry uses the original selected text and context. Network errors retain partial answers as incomplete, give a recovery action, and require an explicit retry.

## Selected player layout

The selected Native Inspector direction uses a neutral charcoal/blue-gray palette: background `#10151b`, inspector/transport `#171d25`, text `#e6eaf2`, muted text `#9aa7b5`, and blue accent `#429df5`. Keep the film dominant; do not restore the large logo banner, repeated title, permanent search settings, tutorial sentence, card counter, or status footer.

The native Mac titlebar contains the filename and two inspector buttons. Other platforms and instant fullscreen use a compact toolbar; fullscreen accounts for the Mac camera safe area. The right inspector is 27.5% of window width, bounded to 280–352 px. Transcript and Meaning tabs sit at its bottom. A single 72 px transport bar spans the full window under the film and inspector.

Previous subtitle, Play/Pause, Next subtitle, and Replay subtitle are separate icon controls before the timeline. Previous/Next play the chosen cue once, then pause near its end, keeping its subtitle visible. Replay plays the current cue once, then pauses near its end, keeping the subtitle visible. A second Replay press stops at the current position and remains paused. Pending seek targets make repeated rapid steps advance through the track rather than repeating the same seek. Transcript row clicks also seek and pause. Keyboard shortcuts are Command/Ctrl–Left, Command/Ctrl–Right, and Command/Ctrl–R. Plain Left/Right seek five seconds and M toggles mute. The speed picker returns focus to playback after selection; Space on the closed speed control toggles playback. These shortcuts never consume search-field text.

The transcript follows the active cue until manually scrolled. Its search icon reveals the existing search and language controls. English/secondary previews use the greatest timing overlap with each primary row; this pairing does not alter either track's independent on-video timing or repair out-of-sync files. Meaning reuses the same functional learning component as the Study drawer, rather than maintaining a second explanation implementation.

Icons are the pinned, ISC-licensed Lucide SVGs in `shared/icons/lucide`; preserve their license. The runtime needs Qt's SVG image plugin, which the Mac installer deploys alongside the SecureTransport backend. Avoid adding handcrafted canvas glyphs or another icon family.

The transcript header has visible Primary and Secondary buttons for loading or replacing two independent tracks. The welcome screen uses a centered 640 px column, the selected A2 icon, Open video, and compact clickable recent rows. File paths appear only in tooltips. Keep Remove available without a separate Resume button or large cards. The app icon master is `shared/branding/neden.png`; its native macOS sizes are in `apps/desktop/macos/Neden.icns`.
