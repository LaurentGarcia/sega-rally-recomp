"""Portable native arithmetic and strict alternate capture boundary checks."""
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
    import check_alternate_orientation as CHECKER
finally:
    sys.path.pop(0)


def capture():
    events = []
    for i, kind in enumerate(CHECKER.PCS):
        regs = [0] * 32
        regs[12], regs[31], regs[5] = 0x500100, 0x220000, 0x220090
        events.append({"schema": 1, "event": kind, "pc": CHECKER.PCS[kind], "call_id": 7,
                       "object": regs[12], "frame": 2300, "instructions": 100 + 100*i,
                       "ac": 0x3f001000, "registers": regs, "object_words": [0] * 256,
                       "blocks": [{"name": "orientation_candidate", "address": regs[5],
                                   "words": [0, 0, 0, 0x913ada01]}]})
    return events


class AlternateOrientationTests(unittest.TestCase):
    def test_host_arithmetic_and_aliasing(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="alternate-orientation-") as directory:
            executable = Path(directory) / "test"
            sources = ["platform/ps2/native/alternate_orientation.cpp", "platform/ps2/native/angle_fold.cpp",
                       "platform/ps2/tests/alternate_orientation_test.cpp"]
            build = subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                                    "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                                    *(str(ROOT / p) for p in sources), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("cases=3728 accepted=3638 hash=f9e5c92050970893", result.stdout)
            self.assertIn("cases=102892 accepted=93943 hash=a57e634ea20576db", result.stdout)
            self.assertIn("REJECTION PASS cases=60 aliases=2", result.stdout)

    def test_complete_capture_includes_folded_candidate_and_result(self):
        rows, provenance = CHECKER.orientation_rows(capture())
        self.assertEqual(rows, [(0,) * 12])
        self.assertEqual(provenance[0]["call_id"], 7)

    def test_incomplete_and_malformed_capture_rejected(self):
        good = capture()
        for bad in ([], good[:2], good[1:], good + good, [good[0], good[2]],
                    [good[0], good[0], good[1], good[2]], [good[0], good[2], good[1]]):
            with self.assertRaises(ValueError):
                CHECKER.orientation_rows(bad)
        changes = [
            lambda e: e[0].__setitem__("pc", 0x5cdce0),
            lambda e: e[0].__setitem__("ac", 0x7f001000),
            lambda e: e[1].__setitem__("call_id", 8),
            lambda e: e[1].__setitem__("object", 0x500200),
            lambda e: e[2].__setitem__("frame", 2299),
            lambda e: e[2].__setitem__("instructions", 1),
            lambda e: e[0]["registers"].__setitem__(5, 0x220094),
            lambda e: e[1]["registers"].__setitem__(12, 0x500200),
            lambda e: e[1]["object_words"].__setitem__(14, 1),
            lambda e: e[2]["object_words"].__setitem__(17, 1),
            lambda e: e[1]["blocks"][0]["words"].__setitem__(3, 1),
            lambda e: e[2]["blocks"][0]["words"].__setitem__(0, 1),
            lambda e: e[0]["blocks"].append(copy.deepcopy(e[0]["blocks"][0])),
            lambda e: e[0].__setitem__("call_id", True),
        ]
        for change in changes:
            bad = copy.deepcopy(good)
            change(bad)
            with self.assertRaises(ValueError):
                CHECKER.orientation_rows(bad)


if __name__ == "__main__":
    unittest.main()
