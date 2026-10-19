#!/usr/bin/env bash
# Development-only Linux LGPL runtime. Run in a disposable prepared build environment.
set -euo pipefail
runtime_prefix="${1:-/opt/neden-lgpl}"
source_directory="${2:-/opt/neden-runtime-sources}"
mkdir -p "$source_directory"
cd "$source_directory"

# 1. Download exact upstream source archives and verify the reviewed bytes.
curl --fail --location https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz -o ffmpeg.tar.xz
curl --fail --location https://github.com/mpv-player/mpv/archive/refs/tags/v0.41.0.tar.gz -o mpv.tar.gz
printf '%s\n' \
  '05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41  ffmpeg.tar.xz' \
  'ee21092a5ee427353392360929dc64645c54479aefdb5babc5cfbb5fad626209  mpv.tar.gz' | sha256sum --check
mkdir -p ffmpeg mpv
tar -xf ffmpeg.tar.xz -C ffmpeg --strip-components=1
tar -xf mpv.tar.gz -C mpv --strip-components=1

# 2. Build shared LGPL codecs, then libmpv against that explicit prefix.
(cd ffmpeg && ./configure --prefix="$runtime_prefix" --enable-shared --disable-static \
  --disable-gpl --disable-nonfree --disable-programs --disable-doc --disable-debug \
  && make -j6 && make install)
export PKG_CONFIG_PATH="$runtime_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$runtime_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
meson setup mpv-build mpv --prefix="$runtime_prefix" --libdir=lib \
  -Dgpl=false -Dlibmpv=true -Dcplayer=false -Dbuild-date=false \
  -Dlua=disabled -Djavascript=disabled -Dlibarchive=disabled
ninja -C mpv-build -j6
ninja -C mpv-build install

# 3. Bind the manifest to the built binary; the CMake check also queries codec licenses.
python3 - "$runtime_prefix" <<'PY'
import hashlib
import json
import pathlib
import sys
prefix = pathlib.Path(sys.argv[1])
library = (prefix / 'lib/libmpv.so').resolve(strict=True)
manifest = {
    'version': '0.41.0', 'gpl': False,
    'sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
    'configuration': '-Dgpl=false -Dlibmpv=true -Dcplayer=false -Dbuild-date=false -Dlua=disabled -Djavascript=disabled -Dlibarchive=disabled',
}
(prefix / 'mpv-build.json').write_text(json.dumps(manifest, indent=2) + '\n')
PY
