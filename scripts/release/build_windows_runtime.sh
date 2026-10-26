#!/usr/bin/env bash
# Run in an MSYS2 UCRT64 shell. Qt/application code uses MSVC; libmpv exposes a C ABI.
set -euo pipefail
runtime_prefix="$(cygpath -u "$1")"
source_directory="$(cygpath -u "$2")"
mkdir -p "$runtime_prefix" "$source_directory"
cd "$source_directory"
curl --fail --location --retry 4 --retry-all-errors --connect-timeout 30 https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz -o ffmpeg.tar.xz
curl --fail --location --retry 4 --retry-all-errors --connect-timeout 30 https://github.com/mpv-player/mpv/archive/refs/tags/v0.41.0.tar.gz -o mpv.tar.gz
printf '%s\n' \
  '05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41  ffmpeg.tar.xz' \
  'ee21092a5ee427353392360929dc64645c54479aefdb5babc5cfbb5fad626209  mpv.tar.gz' | sha256sum --check
mkdir -p ffmpeg mpv
tar -xf ffmpeg.tar.xz -C ffmpeg --strip-components=1
tar -xf mpv.tar.gz -C mpv --strip-components=1
(cd ffmpeg && ./configure --prefix="$runtime_prefix" --enable-shared --disable-static \
  --disable-gpl --disable-nonfree --disable-programs --disable-doc --disable-debug --disable-autodetect \
  && make -j4 && make install)
export PKG_CONFIG_PATH="$runtime_prefix/lib/pkgconfig:/ucrt64/lib/pkgconfig"
export PATH="$runtime_prefix/bin:$PATH"
meson setup mpv-build mpv --prefix="$runtime_prefix" --libdir=lib \
  -Dgpl=false -Dlibmpv=true -Dcplayer=false -Dbuild-date=false \
  -Dlua=disabled -Djavascript=disabled -Dlibarchive=disabled
ninja -C mpv-build -j4
ninja -C mpv-build install
# Keep an exact environment record alongside the unmodified source archives.
pacman -Q > "$runtime_prefix/build-packages.txt"
cp mpv-build/meson-info/intro-buildoptions.json "$runtime_prefix/mpv-build-options.json"
python - "$runtime_prefix" <<'PY'
import hashlib, json, pathlib, sys
prefix = pathlib.Path(sys.argv[1])
libraries = list((prefix / 'bin').glob('*mpv*.dll'))
if len(libraries) != 1:
    raise SystemExit('Expected one source-built libmpv DLL')
library = libraries[0]
(prefix / 'mpv-build.json').write_text(json.dumps({
    'version': '0.41.0', 'gpl': False, 'library': library.name,
    'sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
}, indent=2) + '\n')
PY
