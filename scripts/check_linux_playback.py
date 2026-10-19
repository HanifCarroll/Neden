"""Verify the explicit Linux runtime before linking the player (not a distribution audit)."""
import ctypes
import hashlib
import json
import pathlib
import sys

prefix = pathlib.Path(sys.argv[1]).resolve()
libraries = prefix / 'lib'
checks = {}
for name in ('avcodec', 'avformat', 'avdevice', 'avutil', 'avfilter', 'swresample', 'swscale'):
    path = (libraries / f'lib{name}.so').resolve(strict=True)
    if not path.is_relative_to(prefix):
        raise SystemExit(f'{name}: runtime escaped the audited prefix')
    library = ctypes.CDLL(str(path))
    license_function = getattr(library, f'{name}_license')
    license_function.restype = ctypes.c_char_p
    license_text = license_function().decode()
    configuration_function = getattr(library, f'{name}_configuration')
    configuration_function.restype = ctypes.c_char_p
    configuration = configuration_function().decode()
    if 'LGPL' not in license_text or '--enable-gpl' in configuration or '--enable-nonfree' in configuration:
        raise SystemExit(f'{name}: non-LGPL runtime rejected')
    checks[name] = {'license': license_text, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

path = (libraries / 'libmpv.so').resolve(strict=True)
if not path.is_relative_to(prefix):
    raise SystemExit('mpv: runtime escaped the audited prefix')
mpv = ctypes.CDLL(str(path))
mpv.mpv_create.restype = ctypes.c_void_p
mpv.mpv_initialize.argtypes = [ctypes.c_void_p]
mpv.mpv_get_property_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
mpv.mpv_get_property_string.restype = ctypes.c_void_p
mpv.mpv_free.argtypes = [ctypes.c_void_p]
mpv.mpv_terminate_destroy.argtypes = [ctypes.c_void_p]
handle = mpv.mpv_create()
if not handle or mpv.mpv_initialize(handle) < 0:
    raise SystemExit('Cannot initialize audited mpv')
try:
    # The build manifest is emitted by our source build, not inferred from a package name.
    manifest = json.loads((prefix / 'mpv-build.json').read_text())
    actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
    if manifest.get('gpl') is not False or manifest.get('sha256') != actual_hash:
        raise SystemExit('mpv: reviewed build manifest does not match this binary')
    checks['mpv'] = manifest
finally:
    mpv.mpv_terminate_destroy(handle)
print(json.dumps(checks, indent=2))
