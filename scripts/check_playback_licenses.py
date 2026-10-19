"""Reject unreviewed macOS playback builds; this is not a distribution audit."""
import argparse
import json
from pathlib import Path
import re
import subprocess


REVIEWED_FRAMEWORKS = {
    'Mpv', 'Ass', 'Avcodec', 'Avfilter', 'Avformat', 'Avutil', 'Swresample',
    'Swscale', 'Dav1d', 'Freetype', 'Fribidi', 'Harfbuzz', 'Mbedcrypto',
    'Mbedtls', 'Mbedx509', 'Png16', 'Uchardet', 'Xml2',
}
FFMPEG_FRAMEWORKS = {'Avcodec', 'Avfilter', 'Avformat', 'Avutil', 'Swresample', 'Swscale'}


def inspect_framework(root, name):

    # 1. Inspect every architecture's embedded configuration and license label.
    binary = root / f'{name}.framework' / name
    strings = subprocess.check_output(['strings', str(binary)], text=True)
    configurations = sorted(set(line for line in strings.splitlines()
                                if line.startswith(('-Dgpl=', '--disable-autodetect'))))
    if name == 'Mpv':
        if not configurations or any('-Dgpl=false' not in line for line in configurations):
            raise ValueError('mpv must explicitly disable GPL-only code')
        if '-Dgpl=true' in strings:
            raise ValueError('mpv includes a GPL-enabled configuration')

    if name in FFMPEG_FRAMEWORKS:
        if not configurations or 'license: LGPL version 3 or later' not in strings:
            raise ValueError(f'{name}: missing reviewed LGPL configuration/license')
        if any(flag in strings for flag in ['--enable-gpl', '--enable-nonfree', '--enable-libx264', '--enable-libx265']):
            raise ValueError(f'{name}: GPL/nonfree component enabled')

    # 2. Reject dependencies outside the reviewed framework set and macOS system libraries.
    linkage = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    dependencies = sorted(set(line.strip().split(' (')[0] for line in linkage.splitlines()
                              if line.startswith('\t')))
    for dependency in dependencies:
        if dependency.startswith(('/System/Library/', '/usr/lib/')):
            continue

        match = re.fullmatch(r'@rpath/([A-Za-z0-9]+)\.framework/Versions/A/\1', dependency)
        if match is None or match.group(1) not in REVIEWED_FRAMEWORKS:
            raise ValueError(f'{name}: unreviewed dependency {dependency}')

    return {'framework': name, 'configurations': configurations, 'dependencies': dependencies}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frameworks', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    results = [inspect_framework(args.frameworks, name) for name in sorted(REVIEWED_FRAMEWORKS)]
    if args.report:
        args.report.write_text(json.dumps(results, indent=2) + '\n')

    print('Playback check passed: mpv GPL disabled; FFmpeg LGPL; reviewed dynamic dependencies.')


if __name__ == '__main__':
    main()
