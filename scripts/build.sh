#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo_root/scripts/check_layout.py"
cd "$repo_root/apps/iina-plugin"
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
package="$repo_root/dist/io.github.hanifcarroll.iina-language-learning.iinaplugin"
rm -rf "$package"
mkdir -p "$package/native" "$package/ui" "$package/licenses"
bun run typecheck
swiftc -O native/stream-helper.swift -o "$package/native/stream-helper"
bun build src/main.ts --target browser --format iife --outfile "$package/main.js" >/dev/null
bun build ui/overlay.ts --target browser --format iife --outfile "$package/ui/overlay.js" >/dev/null
bun build ui/sidebar.ts --target browser --format iife --outfile "$package/ui/sidebar.js" >/dev/null
cp Info.json "$package/Info.json"
cp "$repo_root/LICENSE" "$package/LICENSE"
cp ui/overlay.html ui/overlay.css ui/sidebar.html ui/sidebar.css "$package/ui/"
bun "$repo_root/scripts/copy_plugin_notices.ts" "$package"
printf '%s\n' "$package"
