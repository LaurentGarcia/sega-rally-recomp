"""ROM-free tests for strict captured contact-query pairing and native replay."""
import copy
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_driving_contact as CHECKER
finally:
    sys.path.pop(0)


def query(index=0, count=300, call=1, instruction=1, accepted=True, return_pc=0x5cc2d8):
    start = 0x918000
    result = []
    for offset, kind in enumerate(("query_pre", "query_count", "query_post")):
        registers, obj = [0] * 32, [0] * 256
        obj[34] = index & 65535
        registers[16:20] = [2, index & 0xffffffff, 0x510000, 0]
        registers[2] = return_pc
        registers[23] = count
        cursor = start + (0 if kind == "query_pre" else 4)
        if kind == "query_post" and accepted:
            cursor = start + 40
            registers[16], registers[17], registers[29] = (index + count - 1) % count, index * 4, (index + 1) % count
        global_words = [0] * 192
        global_words[149] = 2
        cursor_words = [0] * 16
        cursor_words[4] = cursor
        result.append({"schema": 1, "event": kind,
                       "pc": return_pc if kind == "query_post" else CHECKER.PCS[kind], "call_id": call,
                       "object": 0x210000, "frame": 100, "instructions": instruction + offset,
                       "registers": registers, "object_words": obj,
                       "blocks": [{"name": "globals", "address": 0x214100, "words": global_words},
                                  {"name": "query_stage_global", "address": 0x20a280, "words": cursor_words},
                                  {"name": "query_descriptors", "address": 0x5dcac0, "words": [0] * 24},
                                  {"name": "query_point", "address": 0x510000, "words": [0, 0, 0]}]})
    return result


class DrivingContactTests(unittest.TestCase):
    def test_repeated_query_calls_retain_order_and_signed_rejection(self):
        events = query() + query(-1, instruction=4, accepted=False) + query(299, instruction=7)
        rows, provenance = CHECKER.query_rows(events)
        self.assertEqual(rows, [(300, 0, 1, 299, 0, 1), (300, 65535, 0, 0, 0, 0), (300, 299, 1, 298, 299, 0)])
        self.assertEqual([p["query_ordinal"] for p in provenance], [0, 1, 2])

    def test_missing_duplicate_and_reordered_boundaries_are_rejected(self):
        valid = query()
        for events in ([], valid[:2], valid[1:], [valid[0], valid[2]], valid + valid,
                       [valid[0], valid[0], valid[1], valid[2]], [valid[0], valid[2], valid[1]]):
            with self.subTest(events=len(events)), self.assertRaises(ValueError):
                CHECKER.query_rows(events)

    def test_alternate_call_sites_match_their_own_return_register(self):
        for return_pc in CHECKER.QUERY_RETURN_PCS:
            rows, provenance = CHECKER.query_rows(query(return_pc=return_pc))
            self.assertEqual(rows, [(300, 0, 1, 299, 0, 1)])
            self.assertEqual(provenance[0]["return_pc"], return_pc)
        bad = query()
        bad[2]["pc"] = 0x59f000 + 0x2fc30
        with self.assertRaises(ValueError):
            CHECKER.query_rows(bad)
        with self.assertRaises(ValueError):
            CHECKER.query_rows(query(return_pc=0x123456))

    def test_mismatched_identity_domains_and_cursor_are_rejected(self):
        changes = [(0, "pc", 0), (1, "object", 0x220000), (1, "call_id", 2),
                   (1, "frame", 99), (2, "instructions", 1), (0, "schema", 2)]
        for position, field, value in changes:
            bad = query()
            bad[position][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                CHECKER.query_rows(bad)
        for mutate in (
            lambda rows: rows[0]["registers"].__setitem__(16, 8),
            lambda rows: rows[0]["registers"].__setitem__(17, 1),
            lambda rows: rows[1]["registers"].__setitem__(23, 65536),
            lambda rows: rows[2]["registers"].__setitem__(23, 10),
            lambda rows: rows[2]["registers"].__setitem__(17, 1),
            lambda rows: rows[2]["registers"].__setitem__(16, 65536),
            lambda rows: rows[2]["blocks"][1]["words"].__setitem__(4, 0x918008),
            lambda rows: rows[0]["blocks"].append(copy.deepcopy(rows[0]["blocks"][0])),
            lambda rows: rows[0]["registers"].__setitem__(16, True),
        ):
            bad = query()
            mutate(bad)
            with self.assertRaises(ValueError):
                CHECKER.query_rows(bad)

    def test_native_replay_rejects_mismatch_truncation_and_trailing_data(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="contact-capture-test-") as directory:
            executable, data = Path(directory) / "check", Path(directory) / "records.bin"
            built = subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                                    "-I", str(ROOT / "platform/ps2/native"),
                                    str(ROOT / "platform/ps2/native/contact_neighborhood.cpp"),
                                    str(ROOT / "platform/ps2/tests/contact_capture_check.cpp"), "-o", str(executable)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            valid = b"CONTACT1" + struct.pack("<7I", 1, 300, 0, 1, 299, 0, 1)
            data.write_bytes(valid)
            tested = subprocess.run([str(executable), str(data)], capture_output=True, text=True, timeout=10)
            self.assertEqual(tested.returncode, 0, tested.stderr)
            self.assertIn("NATIVE_CONTACT_CAPTURE PASS", tested.stdout)
            for damaged in (valid[:-1], valid + b"x", valid[:-4] + bytes(4), valid[:8] + bytes(4)):
                data.write_bytes(damaged)
                tested = subprocess.run([str(executable), str(data)], capture_output=True, text=True, timeout=10)
                self.assertNotEqual(tested.returncode, 0)


if __name__ == "__main__":
    unittest.main()
