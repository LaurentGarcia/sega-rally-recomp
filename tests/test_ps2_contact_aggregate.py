"""Original aggregate arithmetic and synthetic main-pass provenance checks."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_contact_aggregate as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_result import main_pass
from test_ps2_contact_query import block


def fixture():
    events, marker = main_pass()
    tail = events.pop()
    template = copy.deepcopy(events[1])
    fp = template["main_frame_pointer"]
    template["ac"] = 0x3f001001  # Positive sign comparison, nearest-even control.
    template["registers"][21] = 0x3f800000
    template["object_words"][5] = 0x3f800000
    template["object_words"][7] = 0x40000000
    template["object_words"][15] = 0x40400000
    template["object_words"][20] = template["object_words"][22] = 0x3f800000
    template["blocks"] = [block("candidate_position", 0x220000, [0, 0, 0]),
                          block("candidate_orientation", 0x220020, [0, 0, 0]),
                          block("response_vectors", fp+0xa0, [0]*12),
                          block("feedback", 0x213960, [7, 10, 20]),
                          block("feedback_enable", 0x2142c8, [0]),
                          block("metric_cross", fp+0xd0, [0, 0x3f800000, 0])]
    for index in range(4):
        for kind, pc in (("metric_post", 0x5d3b5c), ("step_post", 0x5d3b84)):
            event = copy.deepcopy(template)
            event.update(event=kind, pc=pc, step_index=index)
            event["registers"][23] = 12*index
            if kind == "metric_post": event["metric_index"] = index
            events.append(event)
    for kind, pc in (("aggregate_pre", 0x5d3b90), ("aggregate_post", 0x5d3cc0)):
        event = copy.deepcopy(template)
        event.update(event=kind, pc=pc)
        if kind == "aggregate_post":
            event["blocks"][0]["words"] = [0x3f800000, 0, 0x40000000]
            event["blocks"][1]["words"] = [0, 0x40400000, 0]
            event["blocks"][3]["words"] = [7, 0, 0]
        events.append(event)
    events.append(tail)
    for i, event in enumerate(events): event["instructions"] = 10*(i+1)
    marker.update(main_events=len(events), main_step_entries=4, main_step_completions=4,
                  main_step_sampled_pairs=4, main_aggregate_entries=1,
                  main_aggregate_completions=1, main_aggregate_samples=1)
    return events, marker


class ContactAggregateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)

    def test_actual_native_arithmetic_and_preservation(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-aggregate-") as directory:
            executable = Path(directory) / "aggregate"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/contact_aggregate.cpp"),
                       str(ROOT / "platform/ps2/tests/contact_aggregate_test.cpp"), "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout+tested.stderr)
            self.assertIn("CONTACT_AGGREGATE_PROBE PASS steps=2240 updates=912 hash=63248df7338a0797", tested.stdout)
            self.assertIn("CONTACT_AGGREGATE_HOST PASS steps=25728 updates=18400 hash=390cbdf6408065cd", tested.stdout)
            self.assertIn("CONTACT_AGGREGATE_REJECTION PASS checks=33", tested.stdout)

    def test_completed_pipeline_produces_ordered_inputs_and_states(self):
        events, marker = fixture()
        steps, updates, provenance = CHECKER.aggregate_rows(events, marker)
        self.assertEqual(steps, [(0, 0, 0x3f800000, 0)]*4)
        self.assertEqual(len(updates), 1)
        inputs = [0]*13 + [0x3f800000, 0x40000000, 0x40400000, 0x3f800000, 0x3f800000, 1]
        before = [0]*9 + [10, 20]
        after = [0, 0, 0, 0x3f800000, 0, 0x40000000, 0, 0x40400000, 0, 0, 0]
        self.assertEqual(updates[0], tuple(inputs+before+after))
        self.assertEqual([p["kind"] for p in provenance], ["step"]*4+["update"])
        self.assertEqual([p["index"] for p in provenance[:4]], [0, 1, 2, 3])

    def test_frame_pointer_sign_rounding_memory_and_scalar_chain_are_bound(self):
        original, marker = fixture()
        first = next(i for i,e in enumerate(original) if e["event"] == "metric_post")
        changes = [
            lambda e:e[first].__setitem__("pc", 0x5d3b60),
            lambda e:e[first].__setitem__("step_index", 1),
            lambda e:e[first].__setitem__("metric_index", 1),
            lambda e:e[first]["registers"].__setitem__(23, 12),
            lambda e:e[first]["registers"].__setitem__(30, 1),
            lambda e:e[first]["registers"].__setitem__(11, 0x3f800000),
            lambda e:e[first]["registers"].__setitem__(28, 0xbf800000),
            lambda e:e[first]["registers"].__setitem__(28, 0x49800001),
            lambda e:e[first]["registers"].__setitem__(21, 0),
            lambda e:e[first].__setitem__("ac", 0x7f001001),
            lambda e:e[first].__setitem__("ac", 0x3f001004),
            lambda e:e[first]["blocks"][5].__setitem__("address", 0x900000),
            lambda e:e[first]["return_frame"]["words"].__setitem__(2, 0x5d4218),
            lambda e:e[first+1]["registers"].__setitem__(31, 0x200000),
            lambda e:e[first+1]["object_words"].__setitem__(0, 1),
            lambda e:e[first+1]["blocks"][0]["words"].__setitem__(0, 1),
            lambda e:e[first+1]["blocks"][2]["words"].__setitem__(0, 1),
            lambda e:e[first+1]["blocks"][5]["words"].__setitem__(0, 1),
            lambda e:e[first+1].__setitem__("ac", 0x3f001002),
            lambda e:e[first+2]["registers"].__setitem__(11, 1),
        ]
        for number, change in enumerate(changes):
            events = copy.deepcopy(original); change(events)
            with self.subTest(mutation=number), self.assertRaises(ValueError):
                CHECKER.aggregate_rows(events, marker)

    def test_update_preserves_unwritten_fields_and_disabled_feedback(self):
        original, marker = fixture()
        post = next(i for i,e in enumerate(original) if e["event"] == "aggregate_post")
        changes = [
            lambda e:e[post]["object_words"].__setitem__(0, 1),
            lambda e:e[post]["blocks"][0]["words"].__setitem__(1, 1),
            lambda e:e[post]["blocks"][1]["words"].__setitem__(0, 1),
            lambda e:e[post]["blocks"][1]["words"].__setitem__(2, 1),
            lambda e:e[post]["blocks"][2]["words"].__setitem__(0, 1),
            lambda e:e[post]["blocks"][3]["words"].__setitem__(0, 8),
            lambda e:e[post]["blocks"][4]["words"].__setitem__(0, 1),
            lambda e:e[post]["registers"].__setitem__(12, 1),
        ]
        for number, change in enumerate(changes):
            events = copy.deepcopy(original); change(events)
            with self.subTest(mutation=number), self.assertRaises(ValueError):
                CHECKER.aggregate_rows(events, marker)
        events = copy.deepcopy(original)
        for e in events[post-1:post+1]: e["blocks"][4]["words"] = [1]
        with self.assertRaisesRegex(ValueError, "preserved fields"):
            CHECKER.aggregate_rows(events, marker)
        events[post]["blocks"][3]["words"] = [7, 10, 20]
        _, updates, _ = CHECKER.aggregate_rows(events, marker)
        self.assertEqual(updates[0][18], 0)
        self.assertEqual(updates[0][-2:], (10, 20))

    def test_unfinished_reordered_and_overstated_populations_reject(self):
        events, marker = fixture()
        first = next(i for i,e in enumerate(events) if e["event"] == "metric_post")
        variants = [[], events[:-1], events[:first]+events[first+1:],
                    events[:first+1]+events[first+2:], events[:-2]+events[-1:],
                    events[:first+2]+events[first:first+2]+events[first+2:]]
        for bad in variants:
            bad = copy.deepcopy(bad)
            for i,e in enumerate(bad): e["instructions"] = 10*(i+1)
            with self.subTest(events=len(bad)), self.assertRaises(ValueError):
                CHECKER.aggregate_rows(bad, dict(marker, main_events=len(bad)))
        for field in ("main_step_entries", "main_step_completions", "main_step_sampled_pairs",
                      "main_aggregate_entries", "main_aggregate_completions", "main_aggregate_samples"):
            with self.subTest(field=field), self.assertRaises(ValueError):
                CHECKER.aggregate_rows(events, dict(marker, **{field: marker[field]+1}))
        inflated = dict(marker, main_step_entries=8, main_step_completions=8,
                        main_aggregate_entries=2, main_aggregate_completions=2)
        with self.assertRaises(ValueError): CHECKER.aggregate_rows(events, inflated)

    def test_reader_requires_complete_matching_private_evidence(self):
        with tempfile.TemporaryDirectory(prefix="aggregate-proof-", dir=ROOT/"build") as directory:
            parent = Path(directory); capture = parent/"capture"; capture.mkdir()
            events, marker = fixture()
            stream, metadata = capture/"main-contact.jsonl", capture/"capture.json"
            stream.write_text("".join(json.dumps(e)+"\n" for e in events))
            metadata.write_text(json.dumps(marker))
            validation = {"status": "passed", "exact_checkpoint_match": True,
                          "capture_files_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                                   for p in (stream, metadata)}}
            report = parent/"validation.json"; report.write_text(json.dumps(validation))
            steps, updates, _, hashes = CHECKER.read_capture(capture)
            self.assertEqual((len(steps), len(updates), len(hashes)), (4, 1, 3))
            for fields in ({"status": "failed"}, {"exact_checkpoint_match": False}):
                report.write_text(json.dumps(dict(validation, **fields)))
                with self.assertRaisesRegex(ValueError, "observer parity"): CHECKER.read_capture(capture)
            report.write_text(json.dumps(validation)); stream.write_text(stream.read_text()+"\n")
            with self.assertRaisesRegex(ValueError, "identity differs"): CHECKER.read_capture(capture)
            metadata.write_bytes(b" "*16385)
            with self.assertRaisesRegex(ValueError, "bounded size"): CHECKER.read_capture(capture)
        with tempfile.TemporaryDirectory(prefix="aggregate-outside-") as directory:
            with self.assertRaisesRegex(ValueError, "real build directory"): CHECKER.read_capture(Path(directory))


if __name__ == "__main__": unittest.main()
