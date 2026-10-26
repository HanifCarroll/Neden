"""Verify downloaded corresponding-source artifacts before publishing them."""
import argparse
import hashlib
import json
from pathlib import Path


def verify_hash(path, expected):
    with path.open('rb') as source:
        actual = hashlib.file_digest(source, 'sha256').hexdigest()
    if actual != expected:
        raise RuntimeError(f'Source checksum mismatch: {path.name}')


def verify(root):
    manifest_paths = list(root.rglob('manifest.json'))
    if len(manifest_paths) != 1:
        raise RuntimeError('Expected exactly one pinned source manifest')
    manifest = manifest_paths[0]
    rows = json.loads(manifest.read_text())
    for row in rows:
        verify_hash(manifest.parent / row['archive'].replace('\\', '/'), row['sha256'])
        notices = manifest.parent / 'notices' / row['name']
        count = sum(path.is_file() for path in notices.rglob('*'))
        if count != row['noticeFiles'] or count == 0:
            raise RuntimeError(f"Incomplete notices: {row['name']}")
    packages = 0
    for checksum_file in root.rglob('SHA256SUMS'):
        for line in checksum_file.read_text().splitlines():
            expected, filename = line.split(maxsplit=1)
            verify_hash(checksum_file.parent / filename.lstrip('* '), expected)
        metadata = checksum_file.parent / 'metadata'
        for package in (metadata / 'packages.txt').read_text().splitlines():
            notices = checksum_file.parent / 'notices' / package
            if not any(path.is_file() for path in notices.rglob('*')):
                raise RuntimeError(f'Missing package notices: {package}')
            packages += 1
        for archive in (checksum_file.parent / 'archives').glob('*.src.tar.zst'):
            verification = (metadata / (archive.name + '.verification')).read_text()
            if '[GNUPG:] VALIDSIG ' not in verification or not any(
                    f'[GNUPG:] TRUST_{level}' in verification for level in ('FULLY', 'ULTIMATE')):
                raise RuntimeError(f'Missing trusted CI signature verification: {archive.name}')
    print(f'Verified {len(rows)} pinned archives and notice groups; {packages} package dependencies')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    verify(parser.parse_args().directory)
