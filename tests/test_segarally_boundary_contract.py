"""Synthetic exact-chunk capability/link checks; no game ROM or assets."""
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("pipeline", ROOT / "scripts/segarally_recompile.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class BoundaryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(dir=ROOT / "build")
        cls.directory = Path(cls.temp.name)
        cls.tool = pipeline.build_generator(["c++"], cls.directory / "tools")
        program = bytearray(0x100)
        struct.pack_into("<I", program, 12, 0x40)
        # Two independent self branches, split into separate generated chunks.
        for pc in (0x40, 0x80):
            struct.pack_into("<I", program, pc, 0x08000000)
        cls.image = cls.directory / "program.bin"
        cls.image.write_bytes(program)
        cls.seeds = cls.directory / "seeds.txt"
        cls.seeds.write_text("80\n")
        cls.harness = cls.directory / "contract.cpp"
        cls.harness.write_text(r'''
#include "runtime/gen_support.h"
#include <cstdlib>
namespace {
rt::Lockstep::BoundaryAction stop(void *, uint64_t count) {
    return count == 2 ? rt::Lockstep::BoundaryAction::Yield : rt::Lockstep::BoundaryAction::Continue;
}
}
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    bool expected = std::atoi(argv[1]) != 0;
    if (gen::instruction_boundary_hook_compatible() != expected) return 2;
    if (!expected) return 0; // Never execute an incompatible or empty graph.
    // Both labels occupy page zero. Check exact membership plus the alignment
    // and high-page rejection guards, including an unsigned maximum address.
    if (!gen::has_code(0x40) || !gen::has_code(0x80) || gen::has_code(0x41) ||
        gen::has_code(0x1000) || gen::has_code(0xffffffffu)) return 4;
    rt::Cpu cpu(nullptr);
    rt::Lockstep ls(cpu);
    cpu.m_IP = 0x40; ls.end_count = 8;
    ls.set_boundary_hook(stop);
    gen::Env env{cpu, ls};
    gen::run(env);
    return ls.count == 2 && ls.boundary_yielded() && ls.finished() ? 0 : 3;
}
''')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def generate(self, name, enabled=True, extra=()):
        folder = self.directory / name
        folder.mkdir()
        command = [str(self.tool), str(self.image), str(folder), "--board", "model2a",
                   "--chunk", "1", "--seeds", str(self.seeds), "--report", str(folder / "report.json")]
        if enabled:
            command.append("--instruction-boundary-hook")
        result = subprocess.run(command + list(extra), capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return folder

    def compile(self, source, output, defines=()):
        command = ["c++", "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "src"),
                   *["-D" + flag for flag in defines], "-c", str(source), "-o", str(output)]
        if source.name.startswith("chunk_"):
            # The existing generator declares R/AC even for branch-only chunks.
            command.append("-Wno-unused-variable")
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return output

    def link_fixture(self, folder, variant=None, old_chunk=None):
        enabled = ("M2_INSTRUCTION_BOUNDARY_HOOK",)
        objects = []
        for source in sorted(folder.glob("*.cpp")):
            flags = enabled
            if source.name == "chunk_001.cpp":
                if variant == "disabled":
                    flags = ()
                elif variant == "M2_FAST_CPU":
                    # Existing fast_paths.h requires these two together.
                    flags += ("M2_FAST_CPU", "M2_FAST_GEN")
                elif variant:
                    flags += (variant,)
                if old_chunk:
                    source = old_chunk
            output = folder / (source.stem + ".o")
            objects.append(self.compile(source, output, flags))
        for name, source in (("cpu", ROOT / "src/runtime/cpu.cpp"),
                             ("lockstep", ROOT / "src/runtime/lockstep.cpp"),
                             ("harness", self.harness)):
            objects.append(self.compile(source, folder / (name + ".o"), enabled))
        executable = folder / "contract"
        result = subprocess.run(["c++", *map(str, objects), "-o", str(executable)], capture_output=True, text=True)
        return result, executable

    def test_exact_bodies_default_identity_and_enabled_execution(self):
        old = self.generate("plain", False)
        new = self.generate("hook", extra=("--indexed-dispatch",))
        before = json.loads((old / "report.json").read_text())
        after = json.loads((new / "report.json").read_text())
        self.assertTrue(after.pop("instruction_boundary_hook"))
        self.assertEqual(after.pop("instruction_boundary_hook_version"), 1)
        self.assertTrue(after.pop("indexed_dispatch"))
        self.assertEqual(before, after)
        for chunk in old.glob("chunk_*.cpp"):
            source = (new / chunk.name).read_text()
            source = re.sub(r"uint64_t chunk_\d+_boundary_abi\(\) \{.*?\n\}\n\n", "", source, flags=re.S)
            self.assertEqual(chunk.read_text(), source)
        result, executable = self.link_fixture(new)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(executable), "1"]).returncode, 0)

    def test_mixed_disabled_or_inexact_chunk_is_rejected_before_execution(self):
        for variant in ("disabled", "M2_FAST_GEN", "M2_FAST_CPU", "M2_DC_MEMORY"):
            with self.subTest(variant=variant):
                folder = self.generate("mixed-" + variant)
                result, executable = self.link_fixture(folder, variant)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(subprocess.run([str(executable), "0"]).returncode, 0)

    def test_frozen_chunk_without_marker_cannot_link(self):
        old = self.generate("old", False)
        new = self.generate("new")
        result, _ = self.link_fixture(new, old_chunk=old / "chunk_001.cpp")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("chunk_001_boundary_abi", result.stderr)

    def test_empty_graph_has_no_capability(self):
        folder = self.directory / "empty"
        folder.mkdir()
        image = folder / "empty.bin"
        image.write_bytes(bytes(32))
        result = subprocess.run([str(self.tool), str(image), str(folder), "--board", "model2a",
            "--instruction-boundary-hook", "--indexed-dispatch"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(list(folder.glob("chunk_*.cpp")))
        result, executable = self.link_fixture(folder)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(executable), "0"]).returncode, 0)

    def test_generator_rejects_block_mode_or_other_board(self):
        for options in (("--board", "model2a", "--fast_inaccuracy"), ("--board", "model2o")):
            with self.subTest(options=options):
                result = subprocess.run([str(self.tool), str(self.image), str(self.directory),
                    "--instruction-boundary-hook", *options], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn("requires exact Model2A", result.stderr)


if __name__ == "__main__":
    unittest.main()
