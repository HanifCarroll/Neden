"""Reject mixed-revision candidates before creating a public release.

Usage: python3 scripts/release/verify_candidate_records.py COMMIT VERSION DIR [DIR ...]
Each platform artifact directory must contain exactly one release-record.json.
"""
import json
from pathlib import Path
import sys


def verify(commit, version, directories):
    if len(directories) != 3:
        raise ValueError("Provide the macOS, Windows, and Linux candidate directories")
    for directory in directories:
        records = list(Path(directory).rglob("release-record.json"))
        if len(records) != 1:
            raise ValueError(f"Expected one build record in {directory}")
        record = json.loads(records[0].read_text())
        if record != {"commit": commit, "version": version}:
            raise ValueError(f"Candidate revision/version mismatch in {directory}")


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2], sys.argv[3:])
    print("All three candidates match the requested source commit and version.")
