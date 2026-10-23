"""Package the built IINA plugin as a ZIP with files at its root."""
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parent.parent
metadata = json.loads((ROOT / "apps/iina-plugin/Info.json").read_text())
bundle = ROOT / "dist" / f"{metadata['identifier']}.iinaplugin"
archive = bundle.parent / f"{bundle.name}-{metadata['version']}.iinaplgz"

# IINA's pack format is a standard ZIP. ZipFile.write retains executable permissions.
# Reference: https://github.com/iina/iina/blob/develop/iina-plugin/main.swift (packPlugin).
with ZipFile(archive, "w", compression=ZIP_DEFLATED) as package:
    for path in sorted(bundle.rglob("*")):
        relative = path.relative_to(bundle)
        if any(part.startswith(".") or part == "node_modules" for part in relative.parts):
            continue
        if path.is_symlink():
            raise SystemExit(f"Unexpected symbolic link in built plugin: {relative}")
        if path.is_file():
            package.write(path, relative.as_posix())
print(archive)
