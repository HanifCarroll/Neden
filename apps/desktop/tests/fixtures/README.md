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
