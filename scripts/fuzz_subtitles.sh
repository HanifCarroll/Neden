#!/bin/sh
set -eu
repo_root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
build_directory="$repo_root/build/subtitle-fuzz"
duration=${NEDEN_FUZZ_SECONDS:-30}
case "$duration" in ''|*[!0-9]*) printf '%s\n' 'NEDEN_FUZZ_SECONDS must be 1–3600' >&2; exit 2 ;; esac
if [ "$duration" -lt 1 ] || [ "$duration" -gt 3600 ]; then
    printf '%s\n' 'NEDEN_FUZZ_SECONDS must be 1–3600' >&2
    exit 2
fi

# Apple Clang omits libFuzzer; choose an installed upstream LLVM on macOS.
if [ "$(uname -s)" = Darwin ] && [ -z "${CXX:-}" ]; then
    llvm_prefix=$(brew --prefix llvm)
    export CC="$llvm_prefix/bin/clang"
    export CXX="$llvm_prefix/bin/clang++"
else
    export CC="${CC:-clang}"
    export CXX="${CXX:-clang++}"
fi
cmake -S "$repo_root/apps/desktop" -B "$build_directory" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEDEN_BUILD_FUZZERS=ON
cmake --build "$build_directory" --target subtitle_fuzz --parallel "${NEDEN_BUILD_JOBS:-4}"
mkdir -p "$build_directory/corpus" "$build_directory/artifacts"
cp "$repo_root"/apps/desktop/tests/fuzz/corpus/subtitles/* "$build_directory/corpus/"

# Replay all saved cases before mutation. Keep generated inputs outside the source tree.
"$build_directory/subtitle_fuzz" "$build_directory/corpus" -runs=0 -timeout=5 -rss_limit_mb=2048 \
    -artifact_prefix="$build_directory/artifacts/"
"$build_directory/subtitle_fuzz" "$build_directory/corpus" \
    -dict="$repo_root/apps/desktop/tests/fuzz/subtitles.dict" \
    -max_total_time="$duration" -max_len=65536 -timeout=5 -rss_limit_mb=2048 \
    -seed="${NEDEN_FUZZ_SEED:-20261031}" -artifact_prefix="$build_directory/artifacts/"
