# Subtitle tools

Open **Playback options → Subtitles → Translate or repair…** for the primary or secondary track. These tools operate on a frozen copy of that track. Switching media, changing tracks, changing their delay, or changing AI settings cancels pending work so an old result cannot replace a new selection.

## Embedded subtitles

Selecting an embedded text track reads it in the background and makes its text available to the transcript, word lookup, line explanation, repair, and translation. The app uses FFmpeg libraries already included in its reviewed playback runtime; users do not need an `ffmpeg` executable.

Tested formats are Matroska SRT, Matroska ASS, and MP4 `mov_text`. Conversion preserves dialogue and timing but removes ASS layout, styling, and drawing commands. Bitmap subtitles remain available for native display; OCR is not implemented. Extraction currently requires a complete local video, has a two-minute deadline, and supports up to 50,000 cues and 8 MiB of generated text. Network video extraction is not supported.

## Repair synchronization

- **Align to the other track** compares the timing of dialogue across both tracks. The other slot should contain an accurate track from the same cut. It searches offsets up to two minutes in either direction and common film/TV frame-rate conversions, then refines the result. At least 20 cues are required in each track. Weak or ambiguous matches are rejected.
- **Match lines manually** uses a subtitle line and its actual starting time in the video. A second match, at least 30 seconds later, also corrects gradual drift. Video times are entered in seconds; **Use current** copies the current playback position.
- **Apply correction** saves and loads a new SRT. The existing track is unchanged until that action. Existing per-slot delay is baked into the repaired copy, then reset to zero to prevent applying it twice.

This is linear timing repair against another subtitle track or user-chosen anchors. It does not analyze the audio, perform speech recognition, or repair different edits with missing/inserted scenes. Check dialogue near the beginning and end after applying a correction.

## Translate an accurate track

Choose a target language from the language list. Translation retains every cue's start and end time, translating up to eight cues per request with nearby dialogue as context. The confirmation shows the number of lines, estimated requests, and which AI service will receive them. Cloud translation uses one explanation request per batch; a personal provider bills requests under that account.

A completed translation loads into the opposite subtitle slot. It is saved separately, and the source is never overwritten. No partial result is loaded after cancellation, a network failure, a missing cue, or a malformed response. A failed operation currently starts over when retried. AI translation can make mistakes; it preserves timing, not a guarantee of linguistic accuracy. Fix timing before translation.

Generated files live under `subtitles/generated` in the app's profile directory. The normal recent-media history remembers their paths. They can be loaded or copied as ordinary SRT files.

## Ownership and verification

- `EmbeddedSubtitles`: bounded, cancellable FFmpeg text decoding.
- `SubtitleDocument`: SRT serialization, atomic storage, timing transforms and alignment.
- `SubtitleTranslation`: contextual batching, request lifecycle and strict response validation.
- `SubtitleTools`: operation state, background workers and explicit publication.
- `LearningSession`: track selection and application to the current media.
- Hosted `translate` requests: server-owned prompts and the existing account allowance/budget controls.

`bun run test:desktop` covers deterministic translation batches and failures, cancellation, timing correction and ambiguity, preservation of original files, track publication, and extraction from original redistributable media fixtures. `bun run check:api` covers the hosted request boundary and allowance accounting. The opt-in native check `embeddedTextReachesTheInteractiveTranscript` exercises real mpv track metadata and extraction into the transcript. `subtitleToolsRequireExplicitApply` exercises the language picker and explicit repair application, and also runs offscreen.

For an opt-in live translation check, build `subtitle_processing_check`, supply `NEDEN_TRANSLATION_CHECK_KEY` through the process environment, and pass an SRT path followed by a catalog language name. With only an SRT path, the tool validates parsing without any AI request. It uses a temporary profile, never prints credentials, and checks cue counts and every timestamp.
