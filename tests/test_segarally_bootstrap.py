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

    def test_explicit_relocation_retains_guest_relative_targets_and_bounds(self):
        program = bytearray(0x100)
        struct.pack_into("<I", program, 12, 0x40)
        struct.pack_into("<I", program, 0x40, 0x08000008)  # b +8
        struct.pack_into("<I", program, 0x48, 0x0a000000)  # ret
        image = self.directory / "relative.bin"
        image.write_bytes(program)
        seedfile = self.directory / "relative-seeds.txt"
        seedfile.write_text("005a0000\n005a0010\n005a1000\n")
        generated = self.directory / "relative"
        generated.mkdir()
        report = generated / "report.json"
        result = subprocess.run([str(self.tool), str(image), str(generated), "--board", "model2a",
                                 "--relocated-code", "5a0000:40:10", "--seeds", str(seedfile),
                                 "--report", str(report)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        audit = json.loads(report.read_text())
        self.assertIn("0x005a0000", audit["instruction_addresses"])
        self.assertIn("0x005a0008", audit["instruction_addresses"])
        self.assertIn("0x005a0010", audit["unmapped_targets"])
        self.assertIn("0x005a1000", audit["unmapped_targets"])
        self.assertTrue(audit["relocated_code"][0]["runtime_word_guard"])

    def test_invalid_relocated_range_rejected(self):
        for board, relocation in (("model2o", "5a0000:40:10"), ("model2a", "220000:40:10"),
                                  ("model2a", "5ffffc:40:8"), ("model2a", "5a0000:3fffc:8"),
                                  ("model2a", "5a0001:40:10"), ("model2a", "5a0000:40:0")):
            with self.subTest(board=board, relocation=relocation):
                result = subprocess.run([str(self.tool), str(self.directory / "program.bin"),
                    str(self.directory), "--board", board, "--relocated-code", relocation],
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, 2, result.stderr)

    def test_long_instruction_relocation_boundary_requires_real_displacement(self):
        for size in (4, 8):
            with self.subTest(size=size):
                image = self.directory / f"long-{size}.bin"
                program = bytearray(0x80)
                struct.pack_into("<I", program, 12, 0x20)
                struct.pack_into("<I", program, 0x20, 0x0a000000)
                # Self-authored lda absolute fixture. Displacement deliberately
                # resembles another long opcode: it remains plain operand data.
                struct.pack_into("<II", program, 0x40, 0x8c003000, 0x8c003000)
                image.write_bytes(program)
                seedfile = self.directory / f"long-{size}.txt"
                seedfile.write_text("005a0000\n")
                generated = self.directory / f"long-{size}"
                generated.mkdir()
                report = generated / "report.json"
                result = subprocess.run([str(self.tool), str(image), str(generated), "--board", "model2a",
                    "--relocated-code", f"5a0000:40:{size:x}", "--seeds", str(seedfile),
                    "--report", str(report)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                audit = json.loads(report.read_text())
                self.assertEqual("0x005a0000" in audit["instruction_addresses"], size == 8)
                self.assertIn(f"0x005a000{size}", audit["unmapped_targets"])

    def test_indexed_dispatch_matches_address_membership_and_chunk_routing(self):
        # Word holes, page holes, chunk splits within a page, high ROM aliases,
        # and long-instruction displacement words must retain exact membership.
        image = self.directory / "dispatch.bin"
        program = bytearray(0x3010)
        struct.pack_into("<I", program, 12, 0x40)
        for pc in (0x40, 0xffc, 0x1000, 0x3000):
            struct.pack_into("<I", program, pc, 0x0a000000)
        struct.pack_into("<III", program, 0x80, 0x8c003000, 0x12345678, 0x0a000000)
        image.write_bytes(program)
        seeds = self.directory / "dispatch-seeds.txt"
        seeds.write_text("80\nffc\n1000\n3000\n2000000\n6000000\n5a0000\n")
        outputs = {}
        for name, extra in (("baseline", []), ("indexed", ["--indexed-dispatch"])):
            folder = self.directory / ("dispatch-" + name)
            folder.mkdir()
            command = [str(self.tool), str(image), str(folder), "--board", "model2a", "--chunk", "2",
                       "--main-data", str(self.directory / "data.bin"), "--seeds", str(seeds),
                       "--relocated-code", "5a0000:80:c", "--report", str(folder / "report.json"), *extra]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            outputs[name] = (folder, json.loads((folder / "report.json").read_text()))
        baseline, before = outputs["baseline"]
        indexed, after = outputs["indexed"]
        self.assertTrue(after.pop("indexed_dispatch"))
        self.assertEqual(before, after)
        for chunk in baseline.glob("chunk_*.cpp"):
            self.assertEqual(chunk.read_bytes(), (indexed / chunk.name).read_bytes())
        bodies = "\n".join(path.read_text() for path in indexed.glob("chunk_*.cpp"))
        self.assertIn("relocated code differs at 0x005a0000u", bodies)
        addresses = [int(pc, 16) for pc in before["instruction_addresses"]]
        shim = self.directory / "dispatch-shim/runtime"
        shim.mkdir(parents=True)
        (shim / "gen_support.h").write_text("#include <cstdint>\nstruct DispatchCpu { uint32_t m_IP; };\n"
            "namespace gen { struct Env { DispatchCpu &c; }; }\n")
        harness = self.directory / "dispatch-test.cpp"
        source = '#include <cstdint>\n#include <algorithm>\n#include <iterator>\nstruct DispatchCpu { uint32_t m_IP; };\n'
        for name in outputs:
            source += f'namespace {name} {{ struct Env {{ DispatchCpu &c; }}; int selected=-1;\nbool has_code(uint32_t); void run(Env &);\n'
            for i in range(before["chunk_count"]):
                source += f'void chunk_{i:03d}(Env &) {{ selected={i}; }}\n'
            source += '}\n'
        source += 'const uint32_t expected[]={' + ','.join(map(str, addresses)) + '};\n'
        source += '''bool check(uint32_t pc) {
    auto found=std::lower_bound(std::begin(expected),std::end(expected),pc);
    bool valid=found!=std::end(expected) && *found==pc;
    if(baseline::has_code(pc)!=valid || indexed::has_code(pc)!=valid) return false;
    DispatchCpu cpu{pc}; baseline::Env a{cpu}; indexed::Env b{cpu};
    baseline::selected=indexed::selected=-1;
    indexed::run(b);
    if(cpu.m_IP!=pc) return false;
    if(!valid) return indexed::selected==-1;
    baseline::run(a);
    return baseline::selected==int((found-std::begin(expected))/2) && baseline::selected==indexed::selected;
}
int main() {
    for(uint32_t pc=0;pc<0x6010;++pc) if(!check(pc)) return 1;
    for(uint32_t pc:expected) for(int delta=-8;delta<=8;++delta) if(!check(pc+delta)) return 2;
    for(uint32_t pc:{0x400000u,0x5a1000u,0x3000000u,0x7000000u,0xfffffffcu,0xffffffffu}) if(!check(pc)) return 3;
    return 0;
}
'''
        harness.write_text(source)
        objects = []
        for name, (folder, _) in outputs.items():
            obj = folder / "table.o"
            result = subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I" + str(shim.parent), "-Dgen=" + name, "-c", str(folder / "gen_table.cpp"), "-o", str(obj)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            objects.append(str(obj))
        executable = self.directory / "dispatch-test"
        result = subprocess.run(["c++", "-std=c++20", "-O2", str(harness), *objects, "-o", str(executable)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(executable)]).returncode, 0)

    def test_indexed_dispatch_rejects_inexact_block_mode(self):
        result = subprocess.run([str(self.tool), str(self.directory / "program.bin"),
            str(self.directory), "--indexed-dispatch", "--fast_inaccuracy"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("cannot be combined", result.stderr)


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    unittest.main()
