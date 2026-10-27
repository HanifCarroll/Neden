# Translation word breakdowns

New initial explanations request JSON containing a natural translation, a
source-word breakdown, and an ordered alignment between them. Hovering or clicking
an underlined translated word shows its existing source breakdown. Inflected words
and idioms can map to several translated words or several source entries. Hovering
does not send another AI request. Follow-up answers remain ordinary Markdown.

`study/explanation_response` owns the wire-to-readable-text boundary. It emits
only complete fields/array entries while JSON streams in, preserves the complete
translation's spacing, and escapes provider text before making Markdown. Malformed
or truncated JSON is marked incomplete; readable partial content remains available.
Flashcards receive readable Markdown without internal references. Original provider
responses stay in conversation history for follow-ups.

`study/explanation_format` accepts only local references to existing bold source
entries in the translation section. Unknown mappings become plain text; external
URLs and images remain inert. Duplicate source words can show their repeated
entries. Punctuation-only entries are omitted. `ui/ExplanationText.qml` handles the
hover delay, bounded scrollable popup, click alternative, and dismissal on scrolling.

Hosted clients explicitly request `explanationFormat: aligned`. Clients without
that field retain the previous Markdown response, so deploying the API does not
break older releases. Older saved explanations remain readable but have no generated
alignment; request a new explanation to get hover breakdowns.

Validation on October 27, 2026:

- Initial Markdown-reference experiments omitted links and misnumbered entries.
  JSON with explicit source-word references was selected after live DeepSeek tests.
- Turkish → English, Japanese → Spanish, and Arabic → French responses completed
  with references to existing breakdown words (15, 5, and 7 aligned segments).
  One Japanese punctuation segment is deliberately filtered by the reader.
- Automated checks cover every prefix of a streamed JSON fixture, markup escaping,
  missing references, repeated translated words, localized headings, Arabic/Japanese
  text, nested bullets, external links, and legacy responses.
- Actual QML checks exercise hover, click, popup dismissal and scrolling; the portable
  UI acceptance runner includes this scenario.
- All 15 desktop test suites, 13 isolated UI acceptance scenarios, QML lint, and
  all 30 API tests passed. Network integration checks cover a structured initial
  answer, readable flashcard content, ordinary Markdown follow-ups, and truncation.
- The backward-compatible cloud contract was deployed to the invited staging service
  as version `0922cc69-8026-48ae-9eb3-fce44712d317`.

Structural validation cannot prove semantic alignment or linguistic accuracy. The
full breakdown remains available; the model can still make interpretation mistakes.
