# Decoder fixture

`white.mp4` is a generated, silent 1.5-second white frame sequence, covered by
the repository's MIT license. It exercises the actual bundled decoder and
thumbnail renderer without copyrighted media or an external test-time encoder.

Regenerate with FFmpeg:

```sh
ffmpeg -f lavfi -i color=c=white:s=32x18:r=10:d=1.5 -c:v mpeg4 -q:v 5 -an white.mp4
```
