"""Original bounded native direction-blend checks; no original game data."""
from pathlib import Path
import copy
import importlib.util
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("direction_checker", ROOT / "platform/ps2/tools/check_direction_blend.py")
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


def original_events():
    # Original synthetic directions: base(0,1), target(1,0), resulting(1,0).
    registers = [0] * 32
    registers[4:8] = [0, 0x12345678, 0x3F800000, 0x98765432]
    registers[16], registers[17], registers[23] = 0x210000, 0x500100, 0x3F800000
    words = [0] * 256
    words[0x9C // 4] = 0x3F800000
    pre = {"schema": 1, "event": "blend_pre", "pc": CHECKER.PCS["blend_pre"], "call_id": 1,
           "object": registers[16], "frame": 10, "instructions": 1000, "ac": 0x3F001000,
           "registers": registers, "object_words": words,
           "blocks": [{"name": "blend_output", "address": registers[17], "words": registers[4:8]}]}
    store = copy.deepcopy(pre)
    store.update(event="blend_store", pc=CHECKER.PCS["blend_store"], instructions=1017)
    store["registers"][4], store["registers"][6] = 0x3F800000, 0
    post = copy.deepcopy(store)
    post.update(event="blend_post", pc=CHECKER.PCS["blend_post"], instructions=1018)
    post["blocks"][0]["words"] = post["registers"][4:8]
    return [pre, store, post]


class DirectionBlendTests(unittest.TestCase):
    def test_rounding_signed_zero_aliasing_and_atomic_domain_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-direction-") as directory:
            executable = Path(directory) / "direction"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/direction_blend.cpp"),
                       str(ROOT / "platform/ps2/tests/direction_blend_test.cpp"), "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE_DIRECTION_HOST PASS", tested.stdout)
            self.assertIn("NATIVE_DIRECTION_REJECTION PASS", tested.stdout)

    def test_original_capture_triple_and_preserved_words(self):
        rows, provenance, controls = CHECKER.direction_pairs(original_events())
        self.assertEqual(rows, [(0, 0x3F800000, 0x3F800000, 0, 0x3F800000, 0, 10, 1)])
        self.assertEqual(provenance[0]["instructions"], 18)
        self.assertEqual(controls, [0x3F001000])

    def test_capture_rejects_missing_duplicated_or_inconsistent_evidence(self):
        mutations = []
        for removed in range(3):
            records = original_events()
            del records[removed]
            mutations.append(records)
        for repeated in range(3):
            records = original_events()
            records.insert(repeated, copy.deepcopy(records[repeated]))
            mutations.append(records)
        for target, key, value in ((0, "pc", 0), (1, "ac", 0x40000000),
                                    (2, "call_id", 2), (2, "instructions", 1001)):
            records = original_events()
            records[target][key] = value
            mutations.append(records)
        records = original_events()
        records[0]["registers"][18] = 1
        mutations.append(records)
        records = original_events()
        records[1]["object_words"][0] = 1
        mutations.append(records)
        records = original_events()
        records[1]["blocks"][0]["words"][0] = 1
        mutations.append(records)
        records = original_events()
        records[2]["blocks"][0]["words"][1] = 0
        mutations.append(records)
        for index, records in enumerate(mutations):
            with self.subTest(index=index), self.assertRaises(ValueError):
                CHECKER.direction_pairs(records)


if __name__ == "__main__":
    unittest.main()
