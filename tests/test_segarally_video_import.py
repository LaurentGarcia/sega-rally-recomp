#!/usr/bin/env python3
"""Synthetic video ROM fixtures: strict hashes, word/byte lanes and padding."""
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
from segarally_video import import_regions


def load(name, data, offset, operation="ROM_LOAD32_WORD"):
    return {"file": name, "aliases": [name + ".alias"], "size": len(data), "offset": offset,
            "crc32": f"{zlib.crc32(data):08x}", "sha1": hashlib.sha1(data).hexdigest(), "load": operation}


class VideoImportTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build/validation").mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "build/validation")
        self.directory = Path(self.temp.name)
        self.archive = self.directory / "fixture.zip"
        self.data = {"lo.bin": bytes([1, 2, 3, 4]), "hi.bin": bytes([5, 6, 7, 8]),
                     "byte0.bin": bytes([9, 10]), "byte1.bin": bytes([11, 12]),
                     "byte2.bin": bytes([13, 14])}
        self.manifest = {"regions": [
            {"name": "words", "size": 16, "fill": 0,
             "loads": [load(name, self.data[name], lane) for name, lane in (("lo.bin", 0), ("hi.bin", 2))]},
            {"name": "bytes", "size": 12, "fill": 0,
             "loads": [load(name, self.data[name], lane, "ROM_LOAD32_BYTE")
                       for lane, name in enumerate(("byte0.bin", "byte1.bin", "byte2.bin"))]}]}

    def tearDown(self):
        self.temp.cleanup()

    def write_archive(self, values):
        with zipfile.ZipFile(self.archive, "w") as archive:
            for name, data in values.items():
                archive.writestr(name, data)

    def reject(self, data=None, manifest=None):
        self.write_archive(self.data if data is None else data)
        output = self.directory / "rejected"
        with self.assertRaises(ValueError):
            import_regions(self.archive, manifest or self.manifest, output)
        self.assertFalse(output.exists(), "invalid input created derived images")

    def test_word_byte_lanes_padding_and_endianness(self):
        self.write_archive(self.data)
        output = self.directory / "accepted"
        audit = import_regions(self.archive, self.manifest, output)
        words = bytes([1, 2, 5, 6, 3, 4, 7, 8]) + bytes(8)
        byte_lanes = bytes([9, 11, 13, 0, 10, 12, 14, 0]) + bytes(4)
        self.assertEqual((output / "words.bin").read_bytes(), words)
        self.assertEqual((output / "bytes.bin").read_bytes(), byte_lanes)
        self.assertEqual(int.from_bytes(words[:4], "little"), 0x06050201)
        self.assertEqual(audit["images"]["words"]["sha256"], hashlib.sha256(words).hexdigest())
        self.assertEqual(audit["images"]["bytes"]["loaded_bytes"], 6)

    def test_only_declared_exact_content_aliases_accepted(self):
        self.write_archive({name + ".alias": data for name, data in self.data.items()})
        audit = import_regions(self.archive, self.manifest, self.directory / "accepted")
        self.assertEqual(audit["verified_files"][0]["archive_names"], ["lo.bin.alias"])
        self.reject({**self.data, "lo.bin.alias": bytes(4)})
        self.reject({("unknown.bin" if name == "lo.bin" else name): data for name, data in self.data.items()})

    def test_crc_sha1_size_rejected(self):
        for field, value in (("crc32", "00000000"), ("sha1", "0" * 40), ("size", 2)):
            with self.subTest(field=field):
                manifest = copy.deepcopy(self.manifest)
                manifest["regions"][0]["loads"][0][field] = value
                self.reject(manifest=manifest)

    def test_swapped_contents_and_duplicate_basename_rejected(self):
        self.reject({**self.data, "lo.bin": self.data["hi.bin"], "hi.bin": self.data["lo.bin"]})
        self.reject({**self.data, "nested/lo.bin": self.data["lo.bin"]})

    def test_late_region_failure_creates_no_images(self):
        self.reject({**self.data, "byte2.bin": bytes(2)})

    def test_output_outside_ignored_build_rejected(self):
        self.write_archive(self.data)
        with self.assertRaises(ValueError):
            import_regions(self.archive, self.manifest, ROOT / "config")

    def test_invalid_lanes_bounds_and_overlap_rejected(self):
        for offset in (-4, 1, 16):
            with self.subTest(offset=offset):
                manifest = copy.deepcopy(self.manifest)
                manifest["regions"][0]["loads"][0]["offset"] = offset
                self.reject(manifest=manifest)
        manifest = copy.deepcopy(self.manifest)
        manifest["regions"][0]["loads"][1]["offset"] = 0
        self.reject(manifest=manifest)
        manifest = copy.deepcopy(self.manifest)
        manifest["regions"][1]["loads"][2]["offset"] = 10
        self.reject(manifest=manifest)


if __name__ == "__main__":
    unittest.main()
