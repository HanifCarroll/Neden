#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
bun run build
python3 scripts/package_plugin.py
