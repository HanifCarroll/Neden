# Original website sample

`ferry.jpg` was generated for Neden with OpenAI Image Gen on 2026-10-26. It is an illustrative Istanbul ferry scene, not a still from a film or television show. The two subtitle files are original sample dialogue. Website screenshots use the real application and deterministic, locally mocked AI responses; no personal media, profile, or credentials are used.

Make a 42-second sample clip with FFmpeg:

```sh
ffmpeg -loop 1 -i docs/website/sample-media/ferry.jpg -t 42 -r 24 -c:v mpeg4 -q:v 3 -pix_fmt yuv420p /tmp/neden-ferry.mp4
```

Build the opt-in `NedenNativeChecks` target and run `captureWebsiteScreenshots` on a native test display. Set `NEDEN_CAPTURE_WEBSITE=1`, `NEDEN_CHECK_VIDEO` to the clip, `NEDEN_CHECK_PRIMARY`/`NEDEN_CHECK_SECONDARY` to these SRT files, and `NEDEN_CHECK_ARTIFACTS` to an empty output directory. The check uses an isolated temporary profile and no external AI service.
