#!/usr/bin/env python3
"""Synthetic fixtures for strict TGP ROM import and word-lane placement."""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_tgp import import_regions


class ImportTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build/validation").mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "build/validation")
        self.path = Path(self.temp.name)
        self.archive = self.path / "fixture.zip"
        self.data = {"a.bin": bytes([1, 2, 3, 4]), "b.bin": bytes([5, 6, 7, 8])}
        self.manifest = {"regions": [{"name": "fixture", "size": 16, "fill": 0,
            "loads": [{"file": name, "aliases": [name + ".alias"], "offset": offset,
                "size": len(data), "crc32": f"{zlib.crc32(data):08x}",
                "sha1": hashlib.sha1(data).hexdigest(), "load": "ROM_LOAD32_WORD"}
                for offset, (name, data) in zip((0, 2), self.data.items())]}]}

    def tearDown(self):
        self.temp.cleanup()

    def archive_with(self, values):
        with zipfile.ZipFile(self.archive, "w") as file:
            for name, data in values.items():
                file.writestr(name, data)

    def reject(self, values, manifest=None):
        self.archive_with(values)
        output = self.path / "rejected"
        with self.assertRaises(ValueError):
            import_regions(self.archive, manifest or self.manifest, output)
        self.assertFalse(output.exists(), "failed validation wrote derived artifacts")

    def test_lanes_padding_and_valid_aliases(self):
        self.archive_with({name + ".alias": data for name, data in self.data.items()})
        audit, verified = import_regions(self.archive, self.manifest, self.path / "accepted")
        expected = bytes([1, 2, 5, 6, 3, 4, 7, 8]) + bytes(8)
        self.assertEqual((self.path / "accepted/fixture.bin").read_bytes(), expected)
        self.assertEqual(audit["fixture"]["sha256"], hashlib.sha256(expected).hexdigest())
        self.assertEqual(verified[0]["archive_names"], ["a.bin.alias"])

    def test_bad_crc_or_sha1_rejected(self):
        for field in ("crc32", "sha1"):
            with self.subTest(field=field):
                manifest = copy.deepcopy(self.manifest)
                manifest["regions"][0]["loads"][0][field] = "0" * (8 if field == "crc32" else 40)
                self.reject(self.data, manifest)

    def test_bad_present_alias_rejected_even_with_valid_canonical(self):
        self.reject({**self.data, "a.bin.alias": bytes(4)})

    def test_swapped_lanes_rejected_by_content_hash(self):
        self.reject({"a.bin": self.data["b.bin"], "b.bin": self.data["a.bin"]})

    def test_duplicate_basename_rejected(self):
        self.reject({**self.data, "nested/a.bin": self.data["a.bin"]})

    def test_missing_or_wrong_size_rejected(self):
        self.reject({"a.bin": self.data["a.bin"]})
        self.reject({**self.data, "a.bin": self.data["a.bin"][:2]})


if __name__ == "__main__":
    unittest.main()
