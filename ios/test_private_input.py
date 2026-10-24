"""Private-input boundary checks using synthetic data, never a retail disc."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import private_input
from tools.xiso.test_xdvdfs import _build_image


class PrivateInputTests(unittest.TestCase):
    def test_drive_link_validation(self):
        self.assertEqual(private_input.drive_id(
            "https://drive.google.com/file/d/test_ID-123/view?usp=sharing"), "test_ID-123")
        self.assertEqual(private_input.drive_id(
            "https://drive.google.com/open?id=test_ID-123"), "test_ID-123")
        for url in ("http://drive.google.com/file/d/id/view",
                    "https://drive.google.com.attacker.example/file/d/id/view",
                    "https://example.com/?id=secret", ""):
            with self.assertRaises(ValueError):
                private_input.drive_id(url)

    def test_fnv_matches_standard_vector(self):
        self.assertEqual(private_input.fnv1a(b"hello"), 0xA430D84680AABD0B)

    def test_wrong_hash_never_writes_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            iso = base / "synthetic.iso"
            iso.write_bytes(_build_image())
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                private_input.prepare(iso, base / "out", "0" * 64)
            self.assertFalse((base / "out").exists())

    def test_wrong_disc_revision_never_writes_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            iso = base / "synthetic.iso"
            iso.write_bytes(_build_image())
            with self.assertRaises(ValueError):
                private_input.prepare(iso, base / "out")
            self.assertFalse((base / "out").exists())

    def test_extracts_only_validated_executable(self):
        payload = b"XBEH-synthetic-test-input"
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            iso = base / "synthetic.iso"
            data = _build_image(files=(("DEFAULT.XBE", payload), ("private.bin", b"other")))
            iso.write_bytes(data)
            # Only the synthetic fixture's fingerprint is accepted for this test.
            with patch.object(private_input, "XBE_SIZE", len(payload)), \
                 patch.object(private_input, "XBE_FNV", private_input.fnv1a(payload)):
                digest = private_input.prepare(iso, base / "out", hashlib.sha256(data).hexdigest())
            self.assertEqual(digest, hashlib.sha256(data).hexdigest())
            self.assertEqual((base / "out/default.xbe").read_bytes(), payload)
            self.assertEqual([p.name for p in (base / "out").iterdir()], ["default.xbe"])


if __name__ == "__main__":
    unittest.main()
