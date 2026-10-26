"""Check source-built Windows runtime identity and embedded FFmpeg license options."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys

prefix = Path(sys.argv[1]).resolve(strict=True)
with os.add_dll_directory(str(prefix / 'bin')):
    for name in ('avcodec', 'avformat', 'avdevice', 'avutil', 'avfilter', 'swresample', 'swscale'):
        matches = list((prefix / 'bin').glob(f'{name}-*.dll'))
        if len(matches) != 1:
            raise SystemExit(f'Expected one {name} DLL')
        library = ctypes.CDLL(str(matches[0]))
        license_function = getattr(library, f'{name}_license')
        license_function.restype = ctypes.c_char_p
        configuration_function = getattr(library, f'{name}_configuration')
        configuration_function.restype = ctypes.c_char_p
        license_text = license_function().decode()
        configuration = configuration_function().decode()
        if 'LGPL' not in license_text or '--enable-gpl' in configuration or '--enable-nonfree' in configuration:
            raise SystemExit(f'{name}: non-LGPL configuration rejected')
    manifest = json.loads((prefix / 'mpv-build.json').read_text())
    path = (prefix / 'bin' / manifest['library']).resolve(strict=True)
    if not path.is_relative_to(prefix) or manifest.get('gpl') is not False:
        raise SystemExit('Invalid runtime manifest')
    if hashlib.sha256(path.read_bytes()).hexdigest() != manifest['sha256']:
        raise SystemExit('Runtime checksum mismatch')
    options = json.loads((prefix / 'mpv-build-options.json').read_text())
    if not any(option['name'] == 'gpl' and option['value'] is False for option in options):
        raise SystemExit('Meson build options do not establish LGPL mode')
print('Windows playback runtime license configuration and identity verified.')
