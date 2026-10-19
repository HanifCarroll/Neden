"""Check the app layout and the root metadata used by IINA's update service."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
for directory in ("apps/desktop", "apps/iina-plugin", "shared", "docs", "scripts"):
    if not (root / directory).is_dir():
        raise SystemExit(f"Missing project directory: {directory}")

plugin_info = json.loads((root / "apps/iina-plugin/Info.json").read_text())
update_info = json.loads((root / "Info.json").read_text())
if update_info != plugin_info:
    raise SystemExit("Copy apps/iina-plugin/Info.json to root Info.json for IINA update discovery")

print("Project layout and IINA update metadata agree.")
