"""Verify the licensing guard fails closed when runtime evidence changes."""
from pathlib import Path
import unittest
from unittest.mock import patch
from check_playback_licenses import inspect_framework


class PlaybackLicenseTests(unittest.TestCase):
    def inspect(self, name, strings, linkage=''):
        with patch('check_playback_licenses.subprocess.check_output', side_effect=[strings, linkage]):
            return inspect_framework(Path('/fixture'), name)

    def test_rejects_gpl_mpv(self):
        with self.assertRaisesRegex(ValueError, 'disable GPL'):
            self.inspect('Mpv', '-Dgpl=true -Dlibmpv=true')

    def test_rejects_nonfree_even_with_lgpl_label(self):
        with self.assertRaisesRegex(ValueError, 'GPL/nonfree'):
            self.inspect('Avcodec', '--disable-autodetect --enable-nonfree\nlicense: LGPL version 3 or later')

    def test_rejects_missing_evidence(self):
        with self.assertRaisesRegex(ValueError, 'missing reviewed'):
            self.inspect('Avcodec', 'stripped or unknown runtime')

    def test_rejects_unreviewed_transitive_dependency(self):
        with self.assertRaisesRegex(ValueError, 'unreviewed dependency'):
            self.inspect('Ass', '', '\t/opt/homebrew/lib/libunknown.dylib (compatibility version 1.0.0)')

    def test_accepts_reviewed_dynamic_link(self):
        result = self.inspect('Mpv', '-Dgpl=false -Dlibmpv=true',
                              '\t@rpath/Avcodec.framework/Versions/A/Avcodec (compatibility version 60.0.0)')
        self.assertEqual(result['framework'], 'Mpv')


if __name__ == '__main__':
    unittest.main()
