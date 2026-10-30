# Word lookup and selection scope

A word click requests compact cards for the current subtitle, with the clicked
word first. The desktop publishes complete entries while the JSON stream is still
arriving. Clicking another word in that cue shares the request or reads the cache.
Each card initially shows the word, its part of speech and its own meaning.
Details fetches morphology, expression context and distinct senses on demand.

The memory-only cache holds 64 cue contexts. Keys include the clicked track, cue
identity, surrounding dialogue, source/explanation languages and provider settings.
Changing the scene or provider cancels the previous in-flight batch. Closing a card
cancels outstanding work but retains completed entries; sign-out clears the cache.
Requests contain at most 64 distinct words. Unusually long cues can fill additional
misses on later clicks. Failed or truncated responses preserve validated entries,
and missing words remain retryable. No background requests run just from playback.

Cloud clients opt in with `wordFormat: "line"` and a bounded `words` array. Legacy
single-word clients retain their original response. One line request uses one word
lookup allowance; opening uncached Details uses another. Actual token usage is
recorded from the stream. Providers used with a personal key must support streamed
Chat Completions with JSON mode, as initial explanations already require.

Context chooses a word's sense, while the gloss describes that word's own lexical
meaning and inflections. For example, *ettin* contributes “you did/made” to *ima
ettin*; Details explains the combined “you implied.” Separate negation words keep
their own contribution, as in Spanish *no sabía*.

Initial explanations send the highlighted `selection` separately from a `context`
object. Their translation and breakdown cover that selection; the whole cue and
nearby subtitles supply interpretation evidence. “Explain line” still explicitly
selects the full cue. Follow-ups retain their frozen context and normal Markdown.

## Validation — 2026-10-29

- Desktop regression tests cover streamed UTF-8, complete-entry publication,
  unknown/duplicate/oversized entries, rapid same-cue switching, cache hits,
  on-demand Details, language/provider changes, cancellation and partial retry.
- Cloud tests cover opt-in and legacy contracts, bounded word lists, streamed
  response delivery, token accounting, and separating selections from full cues.
- QML acceptance checks exercise actual card gestures, cache reuse and Details.
- Live DeepSeek checks used Turkish and Spanish with English glosses and Japanese
  with Spanish glosses. Four short batches returned all requested words, clicked
  word first, with 89–129 output tokens. First cards took 1.07–1.42 seconds and full
  batches 1.44–1.69 seconds. These are small-sample timings, not latency guarantees.
- The reported Turkish partial selection returned exactly its five selected words
  and the translation “Are you crazy, man? he said.” The live word cards returned
  “you did/made” for *ettin* and “I knew” for *sabía*, keeping adjacent words separate.

Model wording is probabilistic. These fixtures check the reported failures and
request boundaries, not every language or possible expression.
