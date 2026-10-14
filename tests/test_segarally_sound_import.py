#!/usr/bin/env python3
"""Synthetic sound ROM fixtures: hashes, aliases, word order and reset copy."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_sound import build_path, import_regions, reset_vectors, word_swap


class SoundImportTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build/validation").mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "build/validation")
        self.path = Path(self.temp.name)
        self.archive = self.path / "fixture.zip"
        self.data = {"program.bin": bytes(range(16)), "samples.bin": bytes(range(16, 24))}
        self.manifest = {"regions": [
            {"name": region, "size": len(data) * 2, "fill": 0,
             "loads": [{"file": name, "aliases": [name + ".alias"], "offset": 0,
                        "size": len(data), "load": "ROM_LOAD16_WORD_SWAP",
                        "crc32": f"{zlib.crc32(data):08x}",
                        "sha1": hashlib.sha1(data).hexdigest()}]}
            for region, (name, data) in zip(("audiocpu", "samples"), self.data.items())]}

    def tearDown(self):
        self.temp.cleanup()

    def archive_with(self, values):
        with zipfile.ZipFile(self.archive, "w") as roms:
            for name, data in values.items():
                roms.writestr(name, data)

    def reject(self, values, manifest=None):
        self.archive_with(values)
        output = self.path / "rejected"
        with self.assertRaises(ValueError):
            import_regions(self.archive, manifest or self.manifest, output)
        self.assertFalse(output.exists(), "failed validation wrote derived artifacts")

    def test_bus_word_order_padding_and_aliases(self):
        self.archive_with({name + ".alias": data for name, data in self.data.items()})
        audit, verified = import_regions(self.archive, self.manifest, self.path / "accepted")
        expected = bytes([1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14]) + bytes(16)
        self.assertEqual((self.path / "accepted/audiocpu.bin").read_bytes(), expected)
        self.assertEqual(audit["audiocpu"]["sha256"], hashlib.sha256(expected).hexdigest())
        self.assertEqual(verified[0]["archive_names"], ["program.bin.alias"])
        self.assertEqual(reset_vectors(expected), ["0x01000302", "0x05040706", "0x09080b0a", "0x0d0c0f0e"])

    def test_canonical_and_identical_alias_are_both_audited(self):
        self.archive_with({**self.data, "program.bin.alias": self.data["program.bin"]})
        _, verified = import_regions(self.archive, self.manifest, self.path / "accepted")
        self.assertEqual(verified[0]["archive_names"], ["program.bin", "program.bin.alias"])

    def test_bad_hashes_rejected(self):
        for field, length in (("crc32", 8), ("sha1", 40)):
            manifest = copy.deepcopy(self.manifest)
            manifest["regions"][1]["loads"][0][field] = "0" * length
            with self.subTest(field=field):
                self.reject(self.data, manifest)

    def test_bad_present_alias_rejected(self):
        self.reject({**self.data, "program.bin.alias": bytes(16)})

    def test_duplicate_basename_rejected(self):
        self.reject({**self.data, "nested/program.bin": self.data["program.bin"]})

    def test_missing_and_wrong_size_rejected(self):
        self.reject({"program.bin": self.data["program.bin"]})
        self.reject({**self.data, "samples.bin": bytes(6)})

    def test_bad_layouts_rejected_before_output(self):
        for field, value in (("offset", -2), ("offset", 1), ("offset", 18),
                             ("size", 15), ("load", "ROM_LOAD")):
            manifest = copy.deepcopy(self.manifest)
            manifest["regions"][0]["loads"][0][field] = value
            with self.subTest(field=field, value=value):
                self.reject(self.data, manifest)
        manifest = copy.deepcopy(self.manifest)
        manifest["regions"][0]["loads"] *= 2
        self.reject(self.data, manifest)
        manifest = copy.deepcopy(self.manifest)
        manifest["regions"][1]["name"] = "../escape"
        self.reject(self.data, manifest)

    def test_reset_vector_guard(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["reset"] = {"expected_vectors": ["0x00000000"] * 4}
        self.reject(self.data, manifest)
        manifest["reset"]["expected_vectors"] = ["0x01000302", "0x05040706", "0x09080b0a", "0x0d0c0f0e"]
        self.archive_with(self.data)
        import_regions(self.archive, manifest, self.path / "accepted")

    def test_vector_copy_covers_exactly_first_sixteen_bytes(self):
        data = struct.pack(">4I", 0x5000, 0x600100, 0x600682, 0x600682) + b"unrelated trailing bytes"
        self.assertEqual(reset_vectors(data), ["0x00005000", "0x00600100", "0x00600682", "0x00600682"])
        with self.assertRaises(ValueError):
            reset_vectors(bytes(14))
        with self.assertRaises(ValueError):
            word_swap(bytes(3))

    def test_output_must_remain_under_ignored_build(self):
        for path in (ROOT, ROOT / "build", ROOT / "src/sound"):
            with self.assertRaises(ValueError):
                build_path(path)


if __name__ == "__main__":
    unittest.main()
