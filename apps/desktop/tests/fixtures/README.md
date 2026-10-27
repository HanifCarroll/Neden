# Decoder fixture

`white.mp4` is a generated, silent 1.5-second white frame sequence, covered by
the repository's MIT license. It exercises the actual bundled decoder and
thumbnail renderer without copyrighted media or an external test-time encoder.

Regenerate with FFmpeg:

```sh
ffmpeg -f lavfi -i color=c=white:s=32x18:r=10:d=1.5 -c:v mpeg4 -q:v 5 -an white.mp4
```

`white-smoke.mp4` is a separate 30-second generated white clip for installed
application checks. It stays in playback throughout the 20-second pixel-check
deadline, including on software-rendered CI desktops. It uses the same license.

```sh
ffmpeg -f lavfi -i color=c=white:s=160x90:r=10:d=30 -c:v mpeg4 -q:v 5 -an white-smoke.mp4
```

## Embedded subtitle fixtures

`subtitles/source.srt` contains two original test lines, covered by this repository's MIT license.
The small black-video fixtures test embedded SRT, ASS, and MP4 mov_text using the bundled decoder.
No show footage or downloaded subtitles are included. Regenerate from this folder:

```sh
ffmpeg -f lavfi -i color=c=black:s=64x64:d=5 -i subtitles/source.srt -map 0:v -map 1:s -c:v mpeg4 -c:s srt subtitles/text.mkv
ffmpeg -i subtitles/text.mkv -map 0 -c:v copy -c:s ass subtitles/styled.mkv
ffmpeg -i subtitles/text.mkv -map 0 -c:v copy -c:s mov_text subtitles/text.mp4
```
