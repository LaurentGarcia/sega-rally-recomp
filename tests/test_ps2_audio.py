#!/usr/bin/env python3
"""ROM-free sound-image and PCM queue/resampler regressions."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Ps2AudioTests(unittest.TestCase):
    def test_compact_pan_table_and_actual_scsp(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-ps2-scsp-") as temporary:
            results = []
            for compact in (False, True):
                flags = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                         "-I" + str(ROOT / "src")]
                if compact:
                    flags.append("-DM2_COMPACT_SOUND=1")
                for name, sources in (
                    ("pan", ["platform/ps2/tests/scsp_pan_test.cpp"]),
                    ("scsp", ["tests/test_model2_scsp.cpp", "src/runtime/model2_scsp.cpp",
                              "src/runtime/model2_scsp_dsp.cpp", "src/runtime/model2a_clock.cpp"]),
                ):
                    output = Path(temporary) / (name + str(compact))
                    result = subprocess.run(flags + [str(ROOT / path) for path in sources]
                                            + ["-o", str(output)], capture_output=True, text=True, timeout=120)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if name == "pan":
                        fields = dict(word.split("=", 1) for word in result.stdout.split() if "=" in word)
                        self.assertEqual(fields["values"], "131072")
                        self.assertEqual(fields["legacy_ram"], "65536")
                        self.assertEqual(fields["board_ram"], "0")
                        results.append(fields)
            self.assertEqual(results[0]["hash"], results[1]["hash"])
            self.assertEqual(int(results[0]["bytes"]) - int(results[1]["bytes"]), 393216)

    def test_actual_ee_transport_errors_and_lifetime(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-ps2-audio-rpc-") as temporary:
            output = Path(temporary) / "audio_output_test"
            command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                       "-I" + str(ROOT / "platform/ps2/tests/audio_sdk"),
                       "-I" + str(ROOT / "platform/ps2/tests/sdk"),
                       "-I" + str(ROOT / "platform/ps2/include"),
                       str(ROOT / "platform/ps2/tests/audio_output_test.cpp"),
                       str(ROOT / "platform/ps2/src/audio_output.cpp"), "-o", str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("actual EE audio transport mock checks passed", result.stdout)

    def test_paged_sound_and_pcm_stream(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-ps2-audio-") as temporary:
            output = Path(temporary) / "audio_core_test"
            command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                       "-I" + str(ROOT / "src"), "-I" + str(ROOT / "platform/ps2/include"),
                       str(ROOT / "platform/ps2/tests/audio_core_test.cpp"),
                       str(ROOT / "src/runtime/model2a_sound_bus.cpp"), "-o", str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("paged sound, bounded PCM queue and resampler checks passed", result.stdout)

if __name__ == "__main__":
    unittest.main()
