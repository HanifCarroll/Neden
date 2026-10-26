# Original website sample

`ferry.jpg` was generated for Neden with OpenAI Image Gen on 2026-10-26. It is an illustrative Istanbul ferry scene, not a still from a film or television show. The two subtitle files are original sample dialogue. Website screenshots use the real application and deterministic, locally mocked AI responses; no personal media, profile, or credentials are used.

Make a 42-second sample clip with FFmpeg:

```sh
ffmpeg -loop 1 -i docs/website/sample-media/ferry.jpg -t 42 -r 24 -c:v mpeg4 -q:v 3 -pix_fmt yuv420p /tmp/neden-ferry.mp4
```

Build the opt-in `NedenNativeChecks` target and run `captureWebsiteScreenshots` on a native test display. Set `NEDEN_CAPTURE_WEBSITE=1`, `NEDEN_CHECK_VIDEO` to the clip, `NEDEN_CHECK_PRIMARY`/`NEDEN_CHECK_SECONDARY` to these SRT files, and `NEDEN_CHECK_ARTIFACTS` to an empty output directory. The check uses an isolated temporary profile and no external AI service.

## Public website captures

The public website uses actual Neden captures of *Leyla ile Mecnun*, Episode 39,
at **00:52:58.360**, at the user's request. The matching original subtitle lines
are “Nail, vereyim mi asayı sana ha?” and “Nail, shall I give you the staff?”.
The video and complete subtitle tracks are not checked into the repository or
included in downloads. Show imagery retains its original ownership; it is not
covered by Neden's MIT license.

The three views were captured on the Pro's isolated native display at 1440×900.
The UI is real; study responses use a deterministic local fixture so no API keys
or network responses enter the screenshot workflow. Use
`NEDEN_CAPTURE_FIXTURE=docs/website/sample-media/episode-039-fixture.json` with
matching, separately supplied video and subtitle paths. The capture validates the
active primary and secondary cue before taking any image. The fixture's timestamp
is specific to the source episode used for these captures.

Optimized website files are `apps/website/public/images/{player,word,explanation}.webp`.
The generated ferry content above remains an optional, redistribution-friendly
capture fixture for automated testing; it is not used on the public website.
