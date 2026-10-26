"""Recover notices omitted by a binary package from its signed source package."""
import argparse
import io
from pathlib import Path
import shutil
import subprocess
import tarfile

from collect_sources import is_notice


def extract_notices(archive, destination):
    # bsdtar handles MSYS2's zstd container; Python inspects the upstream tarballs.
    archive_tool = shutil.which('bsdtar') or 'tar'
    members = subprocess.check_output([archive_tool, '-tf', str(archive)], text=True).splitlines()
    count = 0
    for name in members:
        if not name.endswith(('.tar.xz', '.tar.gz', '.tar.bz2')):
            continue
        content = subprocess.check_output([archive_tool, '-xOf', str(archive), name])
        with tarfile.open(fileobj=io.BytesIO(content)) as upstream:
            for member in upstream:
                if not member.isfile() or not is_notice(member.name):
                    continue
                output = destination / member.name
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(upstream.extractfile(member).read())
                count += 1
    if not count:
        raise RuntimeError(f'No notices found in {archive}; manual review required')
    print(f'Recovered {count} upstream notices from {archive.name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    extract_notices(args.archive, args.destination)
