#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$REPO_ROOT/prototypes/desktop-comparison/script/build_and_run.sh" "${1:-qt}" "${2:-run}"
