"""Create a verified preview disk image without touching an installed application."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from install_desktop import prepare_bundle


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', required=True, type=Path)
    args = parser.parse_args()
    if not (args.sources / 'manifest.json').is_file():
        raise SystemExit('Verified corresponding sources are required')
    destination = ROOT / 'dist/desktop'
    destination.mkdir(parents=True, exist_ok=True)
    image = destination / 'Neden-macos-arm64-preview.dmg'
    if image.exists():
        raise SystemExit(f'Refusing to overwrite {image}')
    with tempfile.TemporaryDirectory(prefix='neden-package-') as temporary:
        staging = Path(temporary) / 'Neden'
        staging.mkdir()
        prepare_bundle(staging / 'Neden.app', args.sources / 'notices')
        (staging / 'Applications').symlink_to('/Applications')
        subprocess.run(['hdiutil', 'create', '-volname', 'Neden', '-srcfolder', str(staging),
                        '-format', 'UDZO', str(image)], check=True)
    subprocess.run(['hdiutil', 'verify', str(image)], check=True)


if __name__ == '__main__':
    main()
