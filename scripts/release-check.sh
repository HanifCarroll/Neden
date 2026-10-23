#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root/apps/iina-plugin"

bun run lint
bun run format:check
bun test
bun run package

python3 - <<'PY'
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path

info = json.loads(Path("Info.json").read_text())
name = f"{info['identifier']}.iinaplugin"
distribution = Path("../../dist").resolve()
folder = distribution / name
archive = distribution / f"{name}-{info['version']}.iinaplgz"

assert info.get("ghRepo"), "ghRepo must identify the GitHub repository"
assert isinstance(info.get("ghVersion"), int), "ghVersion must be an integer"
assert archive.is_file(), f"Missing archive: {archive}"

with zipfile.ZipFile(archive) as package:
    assert package.testzip() is None, "Archive integrity check failed"
    entries = set(package.namelist())
    required = {
        "Info.json", "main.js", "LICENSE", "native/stream-helper",
        "ui/overlay.html", "ui/overlay.css", "ui/overlay.js",
        "ui/sidebar.html", "ui/sidebar.css", "ui/sidebar.js",
        "licenses/markdown-it.LICENSE",
    }
    assert required <= entries, f"Missing package members: {required - entries}"
    assert json.loads(package.read("Info.json")) == info, "Archive metadata differs from source"
    assert (package.getinfo("native/stream-helper").external_attr >> 16) & 0o111, "Packaged helper lost executable permissions"
    expected_files = {str(path.relative_to(folder)) for path in folder.rglob("*") if path.is_file()}
    archived_files = {name for name in entries if not name.endswith("/")}
    assert archived_files == expected_files, "Archive file list differs from the built plugin"
    for member in sorted(expected_files):
        assert package.read(member) == (folder / member).read_bytes(), f"Archive bytes differ: {member}"

    with tempfile.TemporaryDirectory(prefix="neden-package-check-") as temporary:
        helper = Path(temporary) / "stream-helper"
        helper.write_bytes(package.read("native/stream-helper"))
        helper.chmod(0o755)
        subprocess.run(["python3", "tests/test_stream_helper.py", str(helper)], check=True)
print(f"Release checks passed: {archive}")
PY
