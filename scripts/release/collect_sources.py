"""Collect pinned dependency sources and preserve their license/notice files.

Archives are kept byte-for-byte for publication alongside each installer. This
collects the explicitly pinned libraries; system/package-manager dependencies
must be inventoried separately before a release is published.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
LOCK = Path(__file__).with_name('sources') / 'upstream.json'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def is_notice(name):
    path = PurePosixPath(name)
    filename = path.name.lower()
    return (not path.is_absolute() and '..' not in path.parts
            and (filename.startswith(('license', 'copying', 'copyright', 'notice', 'authors', 'readme'))
                 or filename == 'ftl.txt'
                 or 'LICENSES' in path.parts))


def collect(row, destination):
    # Names come only from the checked-in source lock, never from archive paths.
    archive = destination / 'archives' / (row['name'] + '.source')
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or digest(archive) != row['sha256']:
        partial = archive.with_suffix('.download')
        subprocess.run(['curl', '--fail', '--location', '--retry', '4',
                        '--retry-all-errors', '--connect-timeout', '30',
                        row['url'], '-o', str(partial)], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE)
        if digest(partial) != row['sha256']:
            raise RuntimeError(f"Source checksum mismatch: {row['name']}")
        partial.replace(archive)
    notices = destination / 'notices' / row['name']
    count = 0

    def save(name, data):
        nonlocal count
        if not is_notice(name):
            return
        output = notices / name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
        count += 1

    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                if not member.is_dir() and is_notice(member.filename):
                    save(member.filename, source.read(member))
    else:
        with tarfile.open(archive) as source:
            for member in source:
                if member.isfile() and is_notice(member.name):
                    stream = source.extractfile(member)
                    if stream:
                        save(member.name, stream.read())
    print(f"Verified {row['name']}: {count} notice files", flush=True)
    return {**row, 'archive': str(archive.relative_to(destination)), 'noticeFiles': count}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=['macos', 'linux', 'windows'])
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    rows = [row for row in json.loads(LOCK.read_text()) if args.platform in row['platforms']]
    with ThreadPoolExecutor(max_workers=4) as pool:
        collected = list(pool.map(lambda row: collect(row, args.destination), rows))
    (args.destination / 'manifest.json').write_text(json.dumps(collected, indent=2) + '\n')
    shutil.copy2(ROOT / 'docs/desktop/LIBRARY_REPLACEMENT.md', args.destination / 'README.md')


if __name__ == '__main__':
    main()
