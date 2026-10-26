import json
from pathlib import Path
import tempfile
import unittest
from release.verify_candidate_records import verify


class ReleaseRecordTests(unittest.TestCase):
    def test_mixed_and_missing_candidates_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directories = [Path(temporary) / platform for platform in ("macos", "windows", "linux")]
            for directory in directories:
                directory.mkdir()
                (directory / "release-record.json").write_text(json.dumps({"commit": "a" * 40, "version": "0.2.0"}))
            verify("a" * 40, "0.2.0", directories)
            with self.assertRaises(ValueError):
                verify("b" * 40, "0.2.0", directories)
            with self.assertRaises(ValueError):
                verify("a" * 40, "0.3.0", directories)
            with self.assertRaises(ValueError):
                verify("a" * 40, "0.2.0", directories[:2])
            (directories[0] / "release-record.json").unlink()
            with self.assertRaises(ValueError):
                verify("a" * 40, "0.2.0", directories)
