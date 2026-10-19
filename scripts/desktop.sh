#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command_name=${1:-build}
build_directory="$repo_root/build/desktop"

# 1. Use the same release configuration for normal builds and native tests.
case "$command_name" in
    build|run|test|lint) ;;
    core-test) build_directory="$repo_root/build/desktop-core" ;;
    *) printf '%s\n' 'Usage: scripts/desktop.sh [build|run|test|lint|core-test]' >&2; exit 2 ;;
esac

if [ "$(uname -s)" = Darwin ]; then
    export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
    export QSG_RENDER_LOOP=basic
fi

if [ "$command_name" = core-test ]; then
    cmake -S "$repo_root/apps/desktop" -B "$build_directory" -DCMAKE_BUILD_TYPE=Release -DNEDEN_CORE_ONLY=ON
elif [ "$(uname -s)" = Darwin ]; then
    cmake -S "$repo_root/apps/desktop" -B "$build_directory" -DCMAKE_BUILD_TYPE=Release \
        -DNEDEN_MPV_FRAMEWORKS="${NEDEN_MPV_FRAMEWORKS:-$repo_root/.cache/playback-frameworks}"
else
    cmake -S "$repo_root/apps/desktop" -B "$build_directory" -DCMAKE_BUILD_TYPE=Release \
        -DAUDITED_MPV_PREFIX="${AUDITED_MPV_PREFIX:?Set AUDITED_MPV_PREFIX to the reviewed LGPL runtime}"
fi
cmake --build "$build_directory" --parallel "${NEDEN_BUILD_JOBS:-4}"

# 2. Run only the requested operation against the configured build.
case "$command_name" in
    run)
        if [ "$(uname -s)" = Darwin ]; then
            exec "$build_directory/Neden.app/Contents/MacOS/Neden"
        else
            exec "$build_directory/Neden"
        fi
        ;;
    test|core-test) QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_directory" --output-on-failure ;;
    lint) cmake --build "$build_directory" --target NedenQt_qmllint ;;
esac
