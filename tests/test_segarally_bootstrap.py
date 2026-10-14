"""ROM-independent checks for the Sega Rally import and recompiler maps."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("segarally_recompile", ROOT / "scripts/segarally_recompile.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class ImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)

    def fixture(self, directory, corrupt=False, duplicate=False):
        # Self-authored synthetic boot record; no Sega ROM bytes.
        flat = bytearray(16)
        struct.pack_into("<I", flat, 12, 0x420)
        low = b"".join(flat[i:i+2] for i in range(0, 16, 4))
        high = b"".join(flat[i+2:i+4] for i in range(0, 16, 4))
        loads = []
        for name, lane, data in (("low.rom", 0, low), ("high.rom", 2, high)):
            loads.append({"file": name, "offset": lane, "size": len(data), "load": "ROM_LOAD32_WORD",
                          "crc32": f"{zlib.crc32(data):08x}", "sha1": hashlib.sha1(data).hexdigest()})
        manifest = {"romset": "srallycb", "board": "model2a", "expected_reset_ip": "0x420", "source": {},
                    "regions": [{"name": "program", "size": 64, "fill": 0, "loads": loads}]}
        archive = directory / "fixture.zip"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("low.rom", b"wrong" if corrupt else low)
            output.writestr("high.rom", high)
            if duplicate:
                output.writestr("another/low.rom", low)
        return archive, manifest, bytes(flat)

    def test_word_lanes_and_declared_padding(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temp:
            directory = Path(temp)
            archive, manifest, flat = self.fixture(directory)
            audit = pipeline.extract(archive, manifest, directory / "images")
            imported = (directory / "images/program.bin").read_bytes()
            self.assertEqual(imported, flat + bytes(48))
            self.assertEqual(audit["reset_ip"], "0x00000420")
            self.assertEqual(audit["images"]["program"]["loaded_bytes"], 16)

    def test_checksum_failure_writes_no_images(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temp:
            directory = Path(temp)
            archive, manifest, _ = self.fixture(directory, corrupt=True)
            with self.assertRaisesRegex(ValueError, "verification failed"):
                pipeline.extract(archive, manifest, directory / "images")
            self.assertFalse((directory / "images").exists())

    def test_duplicate_archive_basename_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temp:
            directory = Path(temp)
            archive, manifest, _ = self.fixture(directory, duplicate=True)
            with self.assertRaisesRegex(ValueError, "Ambiguous"):
                pipeline.extract(archive, manifest, directory / "images")


class MappingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(dir=ROOT / "build")
        cls.directory = Path(cls.temp.name)
        cls.tool = pipeline.build_generator(["c++"], cls.directory / "tools")
        program = bytearray(0x40000)
        struct.pack_into("<I", program, 12, 0x40)
        struct.pack_into("<I", program, 0x40, 0x0a000000)  # ret
        struct.pack_into("<I", program, 0x20000, 0x0a000000)
        (cls.directory / "program.bin").write_bytes(program)
        data = bytearray(0x1000004)
        struct.pack_into("<I", data, 0, 0x0a000000)
        struct.pack_into("<I", data, 0x1000000, 0x0a000000)
        (cls.directory / "data.bin").write_bytes(data)
        (cls.directory / "seeds.txt").write_text("00220000\n02000000\n06000000\n04000000\n07000000\n")

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def invoke(self, board):
        generated = self.directory / board
        generated.mkdir()
        report = generated / "report.json"
        result = subprocess.run([str(self.tool), str(self.directory / "program.bin"), str(generated),
                                 "--board", board, "--main-data", str(self.directory / "data.bin"),
                                 "--seeds", str(self.directory / "seeds.txt"), "--report", str(report)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(report.read_text())

    def test_model2a_ram_is_not_daytona_rom_alias(self):
        report = self.invoke("model2a")
        self.assertNotIn("0x00220000", report["instruction_addresses"])
        self.assertIn("0x00220000", report["unmapped_targets"])
        self.assertIn("0x02000000", report["instruction_addresses"])
        self.assertIn("0x06000000", report["instruction_addresses"])
        self.assertIn("0x04000000", report["unmapped_targets"])
        self.assertIn("0x07000000", report["unmapped_targets"])
        self.assertFalse(report["complete_game_coverage"])

    def test_daytona_rom_alias_is_preserved(self):
        report = self.invoke("model2o")
        self.assertIn("0x00220000", report["instruction_addresses"])
        self.assertNotIn("0x00220000", report["unmapped_targets"])


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
