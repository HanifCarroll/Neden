# Local lookup history

Settings → AI & accounts → **Keep a local lookup history (for spaced-repetition
 tools)** opts into an append-only log. Click **Save settings** to apply it. It is
off by default and stored as `learning.lookupHistoryEnabled` in the existing
`study.json` profile (not in a separate INI file).

On macOS the log is `~/Library/Application Support/Neden Qt Lab/lookups.jsonl`.
Custom `--profile` directories keep their log beside their own `study.json`.
There is no migration or backfill of the memory-only word cache.

## JSON Lines contract (version 1)

Each complete UTF-8 line is one compact JSON object:

- `v: 1`, `id` (UUID without braces), `at` (ISO-8601 UTC with milliseconds).
- `kind`: `word`, `details`, `line` (including selection explanations),
  `followup`, or `save`.
- `media` (absolute local path), `positionSec`, `cueText` (raw primary cue text,
  including speaker labels and line breaks), `selection`, `track` (`primary` or
  `secondary`), `sourceLanguage` and `explanationLanguage` (language codes, or
  `auto` when no language is known).
- `cueStartSec` / `cueEndSec` when the selected cue has timing; `secondaryText`
  is the other track's current cue text when available, independently of the AI
  context preference. With a secondary-track gesture the other track is primary.
  `offset` is the clicked UTF-16 text offset for word gestures only.
- Optional `result`: word card fields including `word`, `partOfSpeech`, `meaning`;
  Details morphology/senses fields; line `{translation, breakdown}` with Markdown
  breakdown limited to 4,000 UTF-16 code units without splitting a Unicode pair;
  follow-up `{question, answer}`; save `{success}`.

A gesture is appended immediately without `result`. A completed result appends
another full record with **the same `id`, timestamp, and frozen context**. Consumers
must merge by `id`, with later lines winning. Cache hits also record gestures and
results. Unclicked batch words are not logged. Cancelled, failed, or incomplete
requests may have only the gesture record. Follow-ups and saves keep the original
explained cue even after seeking or changing media.

## Privacy and failure behavior

This feature adds **no network calls**, sync service, or automatic uploads. The log
contains private subtitle text, selections, provider answers, and local media
paths; do not include it in public bug reports. It never records credentials or
request headers. An independently configured spaced-repetition tool can read it;
Neden itself does not send it anywhere.

Turning the setting off prevents new records and later result updates. Already
queued writes can finish. Existing history is retained; to erase it, quit Neden
and delete `lookups.jsonl`. No automatic retention or rotation is applied.

One lazy writer thread appends/flushes/closes files in order; a non-waiting lock
prevents concurrent writers from interleaving records. File/lock failures are
silent and best-effort. The queue is bounded to 256 pending lines, dropping new
records if storage cannot keep up rather than blocking playback. Shutdown drains
queued writes; interrupted processes can lose queued records.

`lookup_history_tests` uses temporary profiles, fake playback, and a loopback AI
fixture to verify serialization and actual study gestures without saved keys,
media files, or paid endpoints. Run `sh scripts/desktop.sh test` and
`sh scripts/desktop.sh lint` from the repository root.
