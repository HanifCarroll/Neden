# Property and fuzz testing

Neden checks subtitle, streaming, session, and profile rules with generated inputs.
These tests complement the example tests and native playback checks. They do not
establish visible video rendering, native fullscreen behavior, or language quality.
All fixtures are synthetic. Tests use temporary profiles and loopback HTTP servers;
they never call AI providers, read credentials, or install the app.

## Property tests

Run the desktop suite with the normal Qt environment:

```sh
sh scripts/desktop.sh core-test
# After building, run only the generated suites:
QT_QPA_PLATFORM=offscreen ctest --test-dir build/desktop-core -L property --output-on-failure
```

The four QtTest executables cover:

| Suite | Rules |
| --- | --- |
| `subtitle_property_tests` | SRT round trips preserve plain text and timestamps; parsing stably sorts overlaps; positive retiming preserves dialogue; integer offsets reverse exactly when no clipping occurs; lookup matches a linear oracle on nonoverlapping tracks; malformed hours and size/count limits are rejected. |
| `stream_property_tests` | Valid SSE below resource limits gives identical text across arbitrary byte partitions, including every split in a Unicode fixture; missing completion, malformed tails, truncation, post-completion data, and oversized frames cannot succeed. |
| `session_property_tests` | Generated explain, deliver, seek, save, stop, track replacement, stale response, and retry commands preserve request ownership and the original saved target. Requests run through the actual session and a held loopback server. |
| `profile_property_tests` | Generated settings/card changes, removal, review, rejection, and reload preserve unrelated fields. Oversized writes and forced filesystem errors preserve memory and saved data. Corrupt documents remain untouched. |

Each QtTest row has an independent seed. Most properties use 64 rows; profile
sequences use 32 and network session sequences use 16. The failing row reports its
seed, and the session suite also reports its action trace. Reproduce a single row:

```sh
NEDEN_PROPERTY_SEED=1313162309 NEDEN_PROPERTY_RUNS=1 \
  build/desktop-core/subtitle_property_tests roundTripsPreservePlainTextAndTimings
```

`NEDEN_PROPERTY_RUNS` accepts 1–10,000; `NEDEN_PROPERTY_SEED` accepts an unsigned
32-bit integer. The C++ generator uses `std::mt19937` and does not automatically
shrink failures. Reduce a failing generated case into a small named regression
test; do not replace the property with that single example. Retiming checks allow
a small floating-point comparison tolerance around millisecond rounding.
CTest caps each property suite at 120 seconds. Invoke an individual executable
directly for larger exploratory runs that need more time.

The IINA plugin uses **fast-check**, pinned in its development dependencies. Its
properties cover UTF-8 sizes, invalid surrogates, fragmented helper frames,
invalid UTF-8, native-stream cancellation, and conversation request ownership.
Failures automatically shrink and report a seed and replay path:

```sh
bun run --cwd apps/iina-plugin test
# Replay a failing property by its exact test name and reported seed/path:
NEDEN_PROPERTY_SEED=1313162309 NEDEN_PROPERTY_PATH='0:1' \
  bun test ./apps/iina-plugin/tests/properties.test.ts -t 'property: helper frames decode identically under arbitrary fragmentation'
```

Omit the replay path for an ordinary run. `NEDEN_PROPERTY_RUNS` also controls
fast-check (default 200 cases per property). Use the reported path and seed from
the actual failure, not the illustrative path above. Existing CI runs both the
desktop and plugin properties as part of their normal checks.

## Subtitle fuzzing

The isolated CMake option `NEDEN_BUILD_FUZZERS=ON` builds `subtitle_fuzz` with
Clang libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer. Both the harness
and Neden parser are instrumented. System Qt libraries are not rebuilt with
sanitizers. The target uses no player, media decoder, network, or profile storage.

On macOS, install upstream LLVM (`brew install llvm`): Apple Clang omits the
libFuzzer runtime. Use the normal Qt CMake prefix. On Ubuntu 24.04, install `clang`,
`libclang-rt-18-dev`, `cmake`, `qt6-base-dev`, and `qt6-declarative-dev`.

```sh
sh scripts/fuzz_subtitles.sh
NEDEN_FUZZ_SECONDS=600 NEDEN_FUZZ_SEED=12345 sh scripts/fuzz_subtitles.sh
```

The runner first replays the saved corpus, then mutates inputs for 30 seconds by
default (configurable from 1–3,600 seconds). It limits individual executions to
5 seconds, RSS to 2 GiB, and generated files to 64 KiB. Larger parser boundaries
are covered by deterministic tests. Normal invalid subtitles may throw
`std::runtime_error`; crashes, violated invariants, timeouts, memory limits, and
sanitizer reports fail the run. `-max_total_time` is a fuzzing budget, not a strict
wall-clock deadline including configuration/build and the last input.

The `Subtitle fuzzing` GitHub workflow runs a 30-second check for changes to the
desktop code and a 10-minute scheduled/manual run. Scheduled/manual runs choose
a seed from the run ID; the fuzzer prints the seed for replay. The job has a
20-minute limit. Logs, failures, and the generated corpus are retained as workflow
artifacts for 14 days. Generated inputs stay under ignored `build/subtitle-fuzz/`;
the checked-in starting corpus is `apps/desktop/tests/fuzz/corpus/subtitles/`.

Reproduce and reduce a crash with the same instrumented build:

```sh
build/subtitle-fuzz/subtitle_fuzz build/subtitle-fuzz/artifacts/crash-HASH
build/subtitle-fuzz/subtitle_fuzz -minimize_crash=1 -max_total_time=30 \
  -exact_artifact_path=build/subtitle-fuzz/reduced.srt build/subtitle-fuzz/artifacts/crash-HASH
```

Keep the original crash artifact until the reduced input is verified to reproduce
the same failure. After fixing it, add the small synthetic input to the checked-in
corpus and add a named ordinary regression test when a behavior assertion is
needed. Retain minimized TypeScript failures as named examples too. Never add
real subtitle collections or user profiles to the corpus.

Initial generated testing exposed literal tag/entity loss during SRT serialization,
unchecked hour conversion, and signed timestamp overflow. Those cases are retained
in the subtitle suite and fuzz corpus. Streaming tests also require malformed data
after `[DONE]` to remain incomplete instead of being accepted as a finished answer.
