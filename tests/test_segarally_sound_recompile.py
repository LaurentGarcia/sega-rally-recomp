#!/usr/bin/env python3
"""Compile synthetic sound programs; test map selection and live RAM guards."""
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SoundGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build/validation").mkdir(parents=True, exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(dir=ROOT / "build/validation")
        cls.path = Path(cls.temp.name)
        cls.compiler = shlex.split(os.environ.get("CXX", "c++"))
        cls.tool = cls.path / "m2sndrecomp"
        cls.compile(["tools/m2sndrecomp/main.cpp", "src/m68k/decode.cpp", "src/m68k/reach.cpp", "src/m68k/timing.cpp"], cls.tool)
        cls.samples = cls.path / "samples.bin"
        cls.samples.write_bytes(bytes(0x800000))

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @classmethod
    def compile(cls, sources, output, extra=()):
        result = subprocess.run(cls.compiler + ["-std=c++20", "-O1", "-Isrc"]
                                + list(extra) + [str(source) for source in sources] + ["-o", str(output)],
                                cwd=ROOT, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def generate(self, name, image, options):
        path = self.path / f"{name}.bin"
        path.write_bytes(image)
        source, audit = self.path / f"{name}.cpp", self.path / f"{name}.json"
        result = subprocess.run([str(self.tool), str(path), str(source), *options, "--audit", str(audit)],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return path, source, json.loads(audit.read_text())

    def test_model2a_runs_real_reset_copy_and_byte_frontier(self):
        image = bytearray(0x80000)
        struct.pack_into(">4I", image, 0, 0x5000, 0x600100, 0x600100, 0x600100)
        # An uninstalled vector beyond the reset copy must not become a seed.
        struct.pack_into(">I", image, 16, 0x600200)
        struct.pack_into(">H", image, 0x200, 0x4afc)  # illegal: fail if falsely reached
        # move.l #$11223344,$20.w; move.b #$5a,$100401.l; bra.s self
        struct.pack_into(">9H", image, 0x100, 0x21fc, 0x1122, 0x3344, 0x0020,
                         0x13fc, 0x005a, 0x0010, 0x0401, 0x60fe)
        path, source, audit = self.generate("model2a", image, ["--board", "model2a"])
        self.assertEqual(audit["seeds"], [0x600100])
        self.assertTrue(audit["live_code_guard"])
        runner = self.path / "boot"
        self.compile(["tools/srally_sound_boot/main.cpp", "src/runtime/snd_cpu.cpp",
                      "src/runtime/model2a_sound_bus.cpp", source], runner)
        run = subprocess.run([str(runner), str(path), str(self.samples), "10"],
                             cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(run.returncode, 3, run.stdout + run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual((result["status"], result["pc"], result["instructions"]), ("frontier", 0x600108, 1))
        self.assertEqual((result["address"], result["data"], result["mask"]), (0x100400, 0x5a, 0x00ff))
        self.assertFalse(result["board_time_modeled"])
        bounded = subprocess.run([str(runner), str(path), str(self.samples), "1"],
                                 cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(bounded.returncode, 4)
        self.assertEqual(json.loads(bounded.stdout)["status"], "instruction_limit")

    def test_live_ram_code_change_stops_before_execution(self):
        image = bytearray(0x80000)
        struct.pack_into(">2I4H", image, 0, 0x5000, 8, 0x4e71, 0x60fe, 0xffff, 0xffff)
        path, source, audit = self.generate("ram", image, ["--board", "model2a"])
        self.assertEqual(audit["seeds"], [8])
        harness = self.path / "ram_guard.cpp"
        harness.write_text(r'''
#include "runtime/model2a_sound_bus.h"
#include "runtime/snd_gen_support.h"
#include <fstream>
#include <iterator>
int main(int argc,char**argv) {
 if(argc!=2) return 2;
 std::ifstream file(argv[1],std::ios::binary);
 const std::vector<uint8_t> program{std::istreambuf_iterator<char>(file),{}};
 const std::vector<uint8_t> samples(0x800000);
 snd::Model2ASoundBus bus(program,samples);
 snd::Cpu68k cpu;cpu.bus=&bus;cpu.reset();
 snd::Sched schedule(cpu);schedule.end_count=1;
 sndgen::Env env{cpu,schedule};
 bus.sound_ram()[9]^=1;
 try {sndgen::run(env);} catch(const rt::Fatal&e) {
  return schedule.count==0 && cpu.pc==8 &&
   std::string(e.what()).find("live sound code differs")!=std::string::npos ? 0:3;
 }
 return 1;
}
''')
        executable = self.path / "ram_guard"
        self.compile([harness, "src/runtime/snd_cpu.cpp", "src/runtime/model2a_sound_bus.cpp", source], executable)
        run = subprocess.run([str(executable), str(path)], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_daytona_default_matches_explicit_daytona(self):
        image = bytearray(0x40000)
        for vector in range(1, 64):
            struct.pack_into(">I", image, vector * 4, 0x100)
        struct.pack_into(">3H", image, 0x100, 0x7001, 0x5240, 0x60fe)
        _, default, audit = self.generate("default", image, [])
        _, explicit, _ = self.generate("daytona", image, ["--board", "daytona"])
        self.assertEqual(default.read_bytes(), explicit.read_bytes())
        self.assertEqual(audit["instructions"], 3)
        self.assertFalse(audit["live_code_guard"])

    def test_timed_scsp_write_has_reference_cycles_and_excludes_code_guard(self):
        image = bytearray(0x80000)
        struct.pack_into(">4I", image, 0, 0x5000, 0x600100, 0x600100, 0x600100)
        # Synthetic move.w #$000f,$100400.l; bra.s self.
        struct.pack_into(">5H", image, 0x100, 0x33fc, 0x000f, 0x0010, 0x0400, 0x60fe)
        path, source, audit = self.generate("timed", image, ["--board", "model2a", "--timing", "model2a-boundary"])
        self.assertEqual(audit["missing_timing"], 0)
        runner = self.path / "timed-boot"
        self.compile(["tools/srally_sound_boot/main.cpp", "src/runtime/snd_cpu.cpp",
                      "src/runtime/model2a_sound_bus.cpp", "src/runtime/model2_scsp.cpp",
                      "src/runtime/model2_scsp_dsp.cpp", "src/runtime/model2a_clock.cpp", source], runner,
                      ["-DSEGARALLY_SOUND_TIMED_PROBE"])
        run = subprocess.run([str(runner), str(path), str(self.samples), "1", "--timed"],
                             cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(run.returncode, 4, run.stdout + run.stderr)
        result = json.loads(run.stdout)
        self.assertEqual(result["instructions"], 1)
        self.assertEqual(result["cycles"], 59)  # reset38 + move20 + oneSCSPwait
        self.assertEqual(result["data_bus_transfers"], 5)
        self.assertEqual(result["modeled_prefetch_transfers"], 6)
        self.assertEqual(result["mapped_wait_cycles"], 5)
        self.assertEqual(result["scsp_writes"], 1)
        self.assertTrue(result["mmio_at_previous_boundary"])
        self.assertFalse(result["bus_phase_verified"])

    def test_unsupported_timing_has_explicit_frontier(self):
        image = bytearray(0x80000)
        struct.pack_into(">4I", image, 0, 0x5000, 0x600100, 0x600100, 0x600100)
        struct.pack_into(">3H", image, 0x100, 0x80fc, 0x0001, 0x60fe)  # divu #1,d0
        _, source, audit = self.generate("divu", image, ["--board", "model2a", "--timing", "model2a-boundary"])
        self.assertEqual(audit["missing_timing"], 1)
        self.assertIn('c.fatal("unsupported sound instruction timing")', source.read_text())

    def test_wrong_model2a_image_size_rejected(self):
        image = self.path / "short.bin"
        image.write_bytes(bytes(16))
        result = subprocess.run([str(self.tool), str(image), str(self.path / "rejected.cpp"),
                                 "--board", "model2a"], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertFalse((self.path / "rejected.cpp").exists())


if __name__ == "__main__":
    unittest.main()
