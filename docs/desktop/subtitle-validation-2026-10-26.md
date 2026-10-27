# Subtitle tools validation — October 26, 2026

Implementation revision: `752f03d`. This is a development-build validation, not a
new public installer release. Usage and limits are in [Subtitle tools](SUBTITLE_TOOLS.md).

## Passing checks

- All 15 desktop test suites passed locally and on the macOS, Linux and Windows
  runners. They include actual Matroska SRT/ASS and MP4 mov_text decoding,
  offset/drift repair, ambiguous-match rejection, translation batching and
  timestamp preservation, invalid responses, cancellation, and separate-file publication.
- All 11 offscreen UI scenarios passed on the platform runners, including the
  translation confirmation and explicit timing-repair application.
- The MacBook Pro passed native embedded-track selection into the transcript,
  paused native ASS selection, and repair application. The dialog and confirmation
  were visually checked at normal size and 900×540 using an isolated profile.
- A missing notification initially left manual line previews blank. Revision
  `4bda688` fixed the binding; the Pro confirmed both previews appear immediately
  without changing line numbers.
- CI exposed cancelled extraction workers briefly occupying the import slots.
  Revision `4549427` queues replacement imports while those workers finish. The
  strengthened regression passed ten additional runs on the Pro at `752f03d`.
- CI also exposed lexical episode ordering under Linux's C locale. Revision
  `752f03d` supplies a numeric-collation fallback; Linux's queue regression passed.
- A live two-cue DeepSeek translation to Spanish preserved both cue timestamps
  and line count. Only original synthetic dialogue was submitted. This verifies
  the request path and structure, not language quality across a full episode.
- 28 hosted API tests, TypeScript checks, QML lint, and 14 packaging/helper tests
  passed. The hosted translation route was deployed to staging and its health
  endpoint checked. An authenticated end-to-end hosted translation was not run.
- The local signed app was installed from `752f03d`, verified with strict deep
  signature validation, and left closed. The normal Pro profile was unchanged.

## Package results and remaining coverage

[Main checks](https://github.com/HanifCarroll/Neden/actions/runs/40434083795)
passed. In the [platform package run](https://github.com/HanifCarroll/Neden/actions/runs/40434084634),
macOS passed its complete packaging job. Linux passed the feature tests but failed
the installed default renderer's white-pixel assertion while playback advanced.
The diagnostic basic-render-loop invocation exited successfully; the workflow
correctly retained the default-renderer failure.

Windows passed all 15 suites and 11 UI scenarios, but failed its installed
white-pixel assertion in both rendering loops in the same final package run.
These visible
playback limitations predate this work and are recorded in the
[earlier acceptance report](validation-2026-10-26.md). Passing subtitle checks does
not qualify those packages as stable.

Bitmap subtitle OCR, audio-based synchronization, nonlinear repair of different
video edits, network-file extraction, and broad translation-quality evaluation
are outside the implemented coverage. Paid subtitle APIs remain deferred; live
free-source sample results are in [Free subtitle sources](FREE_SUBTITLE_SOURCES.md).
