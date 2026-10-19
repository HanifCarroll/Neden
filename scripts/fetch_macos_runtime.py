"""Fetch the pinned LGPL playback frameworks without requiring another app toolchain."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

from check_playback_licenses import REVIEWED_FRAMEWORKS

ARCHIVE_NAME = "libmpv-xcframeworks_v0.6.0_macos-universal-video-default"
ARCHIVE_URL = f"https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.6.0/{ARCHIVE_NAME}.tar.gz"
ARCHIVE_SHA256 = "84d2ad98e046e82c6dc34d8547d76c2afeaee89c0f53032773be8985c95536d6"
ROOT = Path(__file__).resolve().parent.parent


def verify_runtime(directory):
    subprocess.run(
        [sys.executable, str(ROOT / "scripts/check_playback_licenses.py"), str(directory)],
        check=True,
    )


def main():

    # 1. Preserve an existing runtime and require an explicit empty destination for setup.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=ROOT / ".cache/playback-frameworks")
    args = parser.parse_args()
    destination = args.destination.resolve()
    if destination.exists():
        verify_runtime(destination)
        print(f"Using existing reviewed frameworks: {destination}")
        return

    if sys.platform != "darwin":
        raise SystemExit("This runtime is for macOS; use the Linux source recipe on Linux.")

    destination.parent.mkdir(parents=True, exist_ok=True)

    # 2. Verify the upstream archive before extracting or installing any library.
    with tempfile.TemporaryDirectory(prefix="neden-playback-", dir=destination.parent) as temporary:
        staging = Path(temporary)
        archive_path = staging / "playback.tar.gz"
        digest = hashlib.sha256()
        with urllib.request.urlopen(ARCHIVE_URL, timeout=60) as response, archive_path.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != ARCHIVE_SHA256:
            raise SystemExit("Playback archive hash does not match the pinned upstream build.")

        extracted = staging / "extracted"
        with tarfile.open(archive_path) as archive:
            archive.extractall(extracted, filter="data")
        frameworks = staging / "frameworks"
        frameworks.mkdir()
        for name in sorted(REVIEWED_FRAMEWORKS):
            source = extracted / ARCHIVE_NAME / f"{name}.xcframework/macos-arm64_x86_64/{name}.framework"
            shutil.copytree(source, frameworks / source.name, symlinks=True)

        # 3. Check the binaries and apply the local signatures required by Apple silicon.
        verify_runtime(frameworks)
        for name in sorted(REVIEWED_FRAMEWORKS):
            framework = frameworks / f"{name}.framework"
            subprocess.run(
                ["codesign", "--force", "--sign", "-", "--timestamp=none", str(framework)],
                check=True,
            )
            subprocess.run(["codesign", "--verify", "--strict", str(framework)], check=True)
        frameworks.rename(destination)
    print(f"Installed reviewed playback frameworks: {destination}")


if __name__ == "__main__":
    main()
