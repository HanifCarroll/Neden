"""Protect existing installations when configuring a contributor's build."""
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from install_desktop import BUNDLE_ID, validate_install_destination


class InstallDestinationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.bundle = Path(self.directory.name) / "Neden.app"

    def installed_bundle(self, identifier=BUNDLE_ID):
        contents = self.bundle / "Contents"
        contents.mkdir(parents=True)
        (contents / "Info.plist").write_bytes(plistlib.dumps({"CFBundleIdentifier": identifier}))

    def test_new_ad_hoc_install_needs_no_existing_certificate(self):
        validate_install_destination(self.bundle, "-")

    def test_certificate_signed_install_cannot_be_downgraded_to_ad_hoc(self):
        self.installed_bundle()
        with patch('install_desktop.subprocess.run', return_value=subprocess.CompletedProcess(
                [], 0, stderr='Authority=Apple Development: Example\n')):
            with self.assertRaisesRegex(SystemExit, 'signing certificate'):
                validate_install_destination(self.bundle, "-")
            validate_install_destination(self.bundle, "Apple Development: Example")

    def test_ad_hoc_install_can_be_updated(self):
        self.installed_bundle()
        with patch('install_desktop.subprocess.run', return_value=subprocess.CompletedProcess(
                [], 0, stderr='Signature=adhoc\n')):
            validate_install_destination(self.bundle, "-")

    def test_another_app_is_never_replaced(self):
        self.installed_bundle('org.example.other-app')
        with self.assertRaisesRegex(SystemExit, 'Another application'):
            validate_install_destination(self.bundle, "-")

    def test_symlink_and_non_bundle_destinations_are_rejected(self):
        self.bundle.symlink_to(Path(self.directory.name) / 'Elsewhere.app')
        for destination in (self.bundle, Path(self.directory.name)):
            with self.assertRaisesRegex(SystemExit, 'app bundle path'):
                validate_install_destination(destination, "-")


if __name__ == '__main__':
    unittest.main()
