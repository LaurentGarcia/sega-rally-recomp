"""ROM-free exhaustive cabinet byte conversion and private-evidence boundaries."""
import importlib.util
import copy
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("steering_checker", ROOT / "platform/ps2/tools/check_cabinet_steering.py")
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class CabinetSteeringTests(unittest.TestCase):
    @staticmethod
    def pair():
        registers = [0] * 32
        registers[30], registers[31] = 0x5d8a94, 0x200a80
        before = {"schema": 1, "event": "steering_pre", "pair_id": 1, "frame": 2220,
                  "instructions": 1000, "cycles": 2000, "board_ns": 3000,
                  "pc": 0x5d85d8, "ac": 0x3f001000, "input_address": 0x202050,
                  "input_byte": 128, "return_pc": registers[30], "frame_pointer": registers[31],
                  "registers": registers}
        after = copy.deepcopy(before)
        after.update(event="steering_post", instructions=1027, cycles=2100, board_ns=4000,
                     pc=0x5d8664, ac=0x3f001002)
        after["registers"][17], after["registers"][30] = registers[30], 0
        return [before, after]

    def test_live_pair_allows_condition_code_changes_and_records_result(self):
        rows, provenance = CHECKER.steering_pairs(self.pair())
        self.assertEqual(rows, [(128, 0)])
        self.assertEqual(provenance[0]["instructions"], 27)

    def test_live_pair_records_interrupt_inclusive_span_with_a_bound(self):
        pair = self.pair()
        pair[1]["instructions"] = 1101
        pair[1]["registers"][2] = 0x5d8638
        rows, provenance = CHECKER.steering_pairs(pair)
        self.assertEqual(rows, [(128, 0)])
        self.assertTrue(provenance[0]["exceeds_isolated_instruction_span"])
        pair[1]["instructions"] = 5097
        with self.assertRaises(ValueError):
            CHECKER.steering_pairs(pair)

    def test_live_pair_rejects_missing_duplicate_or_changed_identity(self):
        for records in ([], self.pair()[:1], self.pair()[1:], self.pair() * 2):
            with self.assertRaises(ValueError):
                CHECKER.steering_pairs(records)
        for key, value in (("input_byte", 256), ("input_byte", 127), ("return_pc", 8),
                           ("frame_pointer", 8), ("ac", 0x7f001000), ("frame", 2221),
                           ("instructions", 999), ("pc", 0x395d8), ("schema", 2)):
            pair = self.pair()
            pair[1][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                CHECKER.steering_pairs(pair)
        for register, value in ((17, 0), (30, 1)):
            pair = self.pair()
            pair[1]["registers"][register] = value
            with self.assertRaises(ValueError):
                CHECKER.steering_pairs(pair)

    def test_all_samples_rounding_symmetry_center_and_saturation(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-steering-") as directory:
            executable = Path(directory) / "test"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/cabinet_steering.cpp"),
                       str(ROOT / "platform/ps2/tests/cabinet_steering_test.cpp"), "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("PASS cases=256 hash=466a329bdf779c3a", tested.stdout)

    def test_private_output_rejects_exports_and_symlink_escapes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "build").mkdir()
            (root / "outside").mkdir()
            (root / "build/link").symlink_to(root / "outside", target_is_directory=True)
            with patch.object(CHECKER, "ROOT", root):
                self.assertEqual(CHECKER.private_output(root / "build/valid"), root / "build/valid")
                for path in [root, root / "build", root / "outside/evidence", root / "build/link/evidence"]:
                    with self.assertRaises(ValueError):
                        CHECKER.private_output(path)

    def test_wrong_image_is_rejected_before_output_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image, output = root / "placeholder.bin", root / "output"
            image.write_bytes(bytes(32))
            with self.assertRaisesRegex(ValueError, "verified Revision B"):
                CHECKER.check(image, output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
