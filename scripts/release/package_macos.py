"""Create a verified preview disk image without touching an installed application."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from install_desktop import prepare_bundle


def main():
    destination = ROOT / 'dist/desktop'
    destination.mkdir(parents=True, exist_ok=True)
    image = destination / 'Neden-macos-arm64-preview.dmg'
    if image.exists():
        raise SystemExit(f'Refusing to overwrite {image}')
    with tempfile.TemporaryDirectory(prefix='neden-package-') as temporary:
        staging = Path(temporary) / 'Neden'
        staging.mkdir()
        prepare_bundle(staging / 'Neden.app')
        (staging / 'Applications').symlink_to('/Applications')
        subprocess.run(['hdiutil', 'create', '-volname', 'Neden', '-srcfolder', str(staging),
                        '-format', 'UDZO', str(image)], check=True)
    subprocess.run(['hdiutil', 'verify', str(image)], check=True)


if __name__ == '__main__':
    main()
