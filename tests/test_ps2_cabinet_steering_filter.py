"""Portable cabinet filter arithmetic and private capture pairing tests."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_cabinet_steering_filter as CHECKER
finally:
    sys.path.pop(0)


def capture():
    result = []
    for i, kind in enumerate(CHECKER.PCS):
        regs = [0] * 32
        regs[31] = 0x5ff000
        regs[0] = 0x5fef00
        regs[16] = 0x80000000 if i else 0
        caller = [0] * 16
        caller[2] = 0x5d411c
        result.append({"schema": 1, "event": kind, "pc": CHECKER.PCS[kind], "pair_id": 1,
            "frame": 2500, "instructions": 100 + 100*i, "ac": 0x3f001000,
            "frame_pointer": regs[31], "registers": regs,
            "return_frame": {"pc": 0x5d411c, "frame_pointer": 0x5fef00,
                             "source": "ordinary_ram", "cache_index": -1, "words": caller},
            "blocks": [{"name": "steering_history", "address": 0x2142c0, "words": [0]},
                       {"name": "steering_input", "address": 0x202050, "words": [128]}]})
    return result


class CabinetSteeringFilterTests(unittest.TestCase):
    def test_arithmetic_sequences_aliasing_and_invalid_inputs(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="cabinet-steering-filter-") as directory:
            executable = Path(directory) / "test"
            sources = ["platform/ps2/native/cabinet_steering_filter.cpp", "platform/ps2/native/cabinet_steering.cpp",
                       "platform/ps2/tests/cabinet_steering_filter_test.cpp"]
            built = subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                *(str(ROOT / p) for p in sources), "-o", str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stderr)
            self.assertIn("cases=7168 sequence_updates=2048 hash=335d3a428df4150d", tested.stdout)
            self.assertIn("cases=68128 sequence_updates=18432 hash=95145e1d72391f91", tested.stdout)
            self.assertIn("REJECTION PASS cases=2560", tested.stdout)

    def test_complete_filter_pair(self):
        rows, pairs = CHECKER.filter_rows(capture())
        self.assertEqual(rows, [(0, 128, 0, 0x80000000)])
        self.assertEqual(pairs[0]["pair_id"], 1)

    def test_bad_pair_identity_rounding_input_and_return_rejected(self):
        good = capture()
        for bad in ([], good[:1], good[1:], good + good, list(reversed(good))):
            with self.assertRaises(ValueError):
                CHECKER.filter_rows(bad)
        changes = [
            lambda e: e[1].__setitem__("pair_id", 2),
            lambda e: e[1].__setitem__("pc", 0x5d8a90),
            lambda e: e[1].__setitem__("frame", 2499),
            lambda e: e[0].__setitem__("ac", 0x7f001000),
            lambda e: e[1].__setitem__("instructions", 99),
            lambda e: e[1]["registers"].__setitem__(16, 0),
            lambda e: e[1]["return_frame"].__setitem__("pc", 0x5d4118),
            lambda e: e[1]["return_frame"].__setitem__("frame_pointer", 0x5fee00),
            lambda e: e[0]["return_frame"]["words"].__setitem__(2, 0x5d4118),
            lambda e: e[0]["return_frame"].__setitem__("source", "unknown"),
            lambda e: e[0]["return_frame"].__setitem__("cache_index", 3),
            lambda e: e[0]["registers"].__setitem__(0, 0x5fee00),
            lambda e: e[1]["return_frame"]["words"].__setitem__(4, 1),
            lambda e: e[0]["return_frame"].pop("words"),
            lambda e: e[1]["blocks"][1]["words"].__setitem__(0, 127),
            lambda e: e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][0])),
            lambda e: e[1].__setitem__("frame_pointer", 0x5ff100),
        ]
        for change in changes:
            bad = copy.deepcopy(good)
            change(bad)
            with self.assertRaises(ValueError):
                CHECKER.filter_rows(bad)

    def test_completion_and_sample_counts_are_required(self):
        good = {"steering_filter_entries": 601, "steering_filter_completions": 601,
                "steering_filter_samples": 32}
        CHECKER.validate_capture_counts(good, 32)
        for field, value in (("steering_filter_entries", 0), ("steering_filter_completions", 600),
                             ("steering_filter_samples", 33), ("steering_filter_samples", True)):
            bad = dict(good)
            bad[field] = value
            with self.assertRaises(ValueError):
                CHECKER.validate_capture_counts(bad, 32)
        with self.assertRaises(ValueError):
            CHECKER.validate_capture_counts({}, 32)
        with self.assertRaises(ValueError):
            CHECKER.validate_capture_counts({"steering_filter_entries": 1,
                "steering_filter_completions": 1, "steering_filter_samples": 2}, 2)


if __name__ == "__main__":
    unittest.main()
