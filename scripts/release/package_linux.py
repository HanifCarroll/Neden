"""Create a Debian installer from a staged Qt deployment (Ubuntu 24.04 x86-64)."""
import argparse
import configparser
from pathlib import Path
import re
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def system_dependencies(staging):
    dependencies = set()
    inspected_libraries = set()
    environment = os.environ.copy()
    environment.pop('LD_LIBRARY_PATH', None)
    environment.pop('LD_PRELOAD', None)
    for binary in staging.rglob('*'):
        if not binary.is_file() or binary.is_symlink():
            continue
        with binary.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                continue
        result = subprocess.run(['ldd', str(binary)], text=True, capture_output=True, env=environment)
        if 'not found' in result.stdout:
            raise RuntimeError(f'Unresolved library in {binary}: {result.stdout}')
        for match in re.finditer(r'(?:=>\s+)?(/[^\s]+)', result.stdout):
            library = Path(match.group(1)).resolve()
            if library.is_relative_to(staging.resolve()):
                continue
            if library in inspected_libraries:
                continue
            inspected_libraries.add(library)
            # A runtime path into a build checkout is never a system dependency.
            if not str(library).startswith(('/usr/lib/', '/lib/')):
                raise RuntimeError(f'Unbundled dependency: {library}')
            # Ubuntu's merged /usr can resolve a path differently from dpkg's inventory.
            candidates = [str(library)]
            if str(library).startswith('/usr/lib/'):
                candidates.append(str(library).removeprefix('/usr'))
            for candidate in candidates:
                ownership = subprocess.run(['dpkg-query', '-S', candidate], text=True, capture_output=True)
                if ownership.returncode == 0:
                    break
            else:
                raise RuntimeError(f'No distribution package owns {library}')
            owner = ownership.stdout.split(': ')[0]
            package = subprocess.check_output(
                ['dpkg-query', '-W', '-f=${binary:Package} (>= ${Version})', owner], text=True)
            dependencies.add(package)
    return sorted(dependencies)


def stage_webengine(staging, qt_prefix):
    """Qt WebView loads Chromium indirectly, so its helper/resources need staging."""
    # Neden does not read serial GPS devices. Qt deploy copies this optional
    # positioning plugin even when the SerialPort module is not installed.
    (staging / 'plugins/position/libqtposition_nmea.so').unlink(missing_ok=True)
    helper = staging / 'libexec/QtWebEngineProcess'
    helper.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(qt_prefix / 'libexec/QtWebEngineProcess', helper)
    shutil.copytree(qt_prefix / 'resources', staging / 'resources', dirs_exist_ok=True)
    shutil.copytree(qt_prefix / 'translations/qtwebengine_locales',
                    staging / 'translations/qtwebengine_locales', dirs_exist_ok=True)
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(staging / 'bin/qt.conf')
    for key, value in {'Prefix': '..', 'LibraryExecutables': 'libexec',
                       'Data': '.', 'Translations': 'translations'}.items():
        config['Paths'][key] = value
    with (staging / 'bin/qt.conf').open('w') as output:
        config.write(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('staging', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--qt-prefix', type=Path, required=True)
    args = parser.parse_args()
    staging = args.staging.resolve(strict=True)
    stage_webengine(staging, args.qt_prefix.resolve(strict=True))
    version = (ROOT / 'apps/desktop/VERSION.txt').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise SystemExit('Invalid desktop version')
    # Every private ELF resolves dependencies relative to the installed tree.
    for binary in staging.rglob('*'):
        if not binary.is_file() or binary.is_symlink():
            continue
        with binary.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                continue
        library_path = os.path.relpath(staging / 'lib', binary.parent)
        subprocess.run(['patchelf', '--set-rpath', '$ORIGIN:$ORIGIN/' + library_path,
                        str(binary)], check=True)
    dependencies = system_dependencies(staging)
    with tempfile.TemporaryDirectory(prefix='neden-deb-') as temporary:
        package = Path(temporary)
        shutil.copytree(staging, package / 'opt/neden', symlinks=True)
        (package / 'DEBIAN').mkdir()
        size = sum(p.stat().st_size for p in staging.rglob('*') if p.is_file()) // 1024
        (package / 'DEBIAN/control').write_text(
            f'Package: neden\nVersion: {version}~preview1\nArchitecture: amd64\n'
            'Maintainer: Neden contributors\n'
            'Section: video\nPriority: optional\n'
            f'Installed-Size: {size}\nDepends: {", ".join(dependencies)}\n'
            'Homepage: https://neden.hanifcarroll.com\n'
            'Description: Video player for learning languages\n'
            ' Watch local videos with dual subtitles and contextual explanations.\n')
        launchers = package / 'usr/bin'; launchers.mkdir(parents=True)
        (launchers / 'neden').symlink_to('/opt/neden/bin/Neden')
        desktop = package / 'usr/share/applications'; desktop.mkdir(parents=True)
        (desktop / 'neden.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Neden\nExec=neden %U\n'
            'Icon=neden\nTerminal=false\nCategories=AudioVideo;Player;Education;\n')
        icons = package / 'usr/share/icons/hicolor/256x256/apps'; icons.mkdir(parents=True)
        shutil.copy2(ROOT / 'shared/branding/neden.png', icons / 'neden.png')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(package), str(args.output)], check=True)
    subprocess.run(['dpkg-deb', '--info', str(args.output)], check=True)


if __name__ == '__main__':
    main()
