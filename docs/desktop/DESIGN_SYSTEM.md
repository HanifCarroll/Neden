# Desktop design system

`apps/desktop/ui/Theme.qml` is the source of truth for colors, type sizes, spacing, and corner radii. Use its tokens when adding or changing a surface. Do not add tiny all-caps category headings, emoji, or decorative labels above titles.

- Typography: Inter, 12 px captions, 14 px body/controls, 20 px section titles, 24 px page headings. Use regular for body copy and semibold for headings.
- Spacing: 4, 8, 12, 16, 24, 32 px. Panels use 16 or 24 px padding; related controls use 8 or 12 px gaps.
- Shape: 8 px controls, 12 px panels. Keep the video surface free of decorative backgrounds and blur effects.
- Controls: `ActionButton`, `SearchField`, `ChoiceBox`, `ControlSlider`, and `SegmentButton`. ChoiceBox explicitly sets both highlighted text and background colors; do not mix default light delegates into the dark UI.
- Native menus: File, Playback, Subtitles, Study, View. macOS uses the system menu bar; other platforms display Qt's menu bar. Keep playback, volume, and seeking near the video. Put secondary actions in menus.

`SubtitleText.qml` shares one text layout for the visible outline and selectable text, for both tracks. The default is white semibold text, a dark outline, a 28 px primary line, a 24 px secondary line, and a 36 px bottom margin. There is no background by default. Settings offers independent sizes, margin, color, outline, weight, and an optional compact background; appearance lives in `appearance.ini`, separately from recent videos. Never persist the user's subtitle track choices.

Subtitle parsing removes recognized SRT presentation tags (`i`, `b`, `u`, `s`, `font`, and line breaks) and decodes common named entities. It does not execute HTML or load resources; unknown tags remain inert literal text. This intentionally applies the user's chosen typography uniformly, instead of honoring italics or colors embedded in a subtitle file.

Selection pauses playback immediately. A completed mouse selection or Shift-key selection opens Study and makes one explanation request. Changing the selection cancels the previous response; closing the panel restores the previous playback state. Dragging does not submit a request for every intermediate selection. Network errors retain partial answers as incomplete, give a recovery action, and require an explicit retry.
