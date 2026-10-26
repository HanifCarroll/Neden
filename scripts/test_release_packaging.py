"""Regression checks for relocatable Linux dependency inspection."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    'package_linux', Path(__file__).parent / 'release/package_linux.py')
package_linux = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package_linux)


class LinuxDependencyTests(unittest.TestCase):
    def test_build_environment_does_not_override_installed_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            staging = Path(directory)
            binary = staging / 'Neden'
            binary.write_bytes(b'\x7fELF')
            with patch.dict('os.environ', {'LD_LIBRARY_PATH': '/build/Qt/lib', 'LD_PRELOAD': '/build/inject.so'}), \
                 patch.object(package_linux.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, '', '')) as run:
                self.assertEqual(package_linux.system_dependencies(staging), [])
                environment = run.call_args.kwargs['env']
                self.assertNotIn('LD_LIBRARY_PATH', environment)
                self.assertNotIn('LD_PRELOAD', environment)

    def test_dependency_outside_package_and_system_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            staging = Path(directory)
            (staging / 'Neden').write_bytes(b'\x7fELF')
            output = subprocess.CompletedProcess([], 0, 'libQt6Quick.so => /build/Qt/libQt6Quick.so (0x1)', '')
            with patch.object(package_linux.subprocess, 'run', return_value=output):
                with self.assertRaisesRegex(RuntimeError, 'Unbundled dependency'):
                    package_linux.system_dependencies(staging)


if __name__ == '__main__':
    unittest.main()
