"""Record the immutable source identity beside each candidate installer."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
if revision != sys.argv[1]:
    raise SystemExit("Checkout does not match the requested release revision")
record = {"commit": revision, "version": (root / "apps/desktop/VERSION.txt").read_text().strip()}
Path(sys.argv[2]).write_text(json.dumps(record, indent=2) + "\n")
