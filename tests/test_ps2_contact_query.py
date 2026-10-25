"""Synthetic contact replies and strict private-capture provenance, without ROMs."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_contact_query as CHECKER
finally:
    sys.path.pop(0)

POINT = [0x3e800000, 0x3f000000, 0xbf400000]
PAYLOAD = list(range(0x3f000010, 0x3f000021))
CALLER_FP, HELPER_FP = 0x200a80, 0x200b80
POINT_ADDRESS, OUTPUT_ADDRESS = 0x510000, 0x200ac0


def saved_frame(pc, source="ordinary_ram"):
    words = [0] * 16
    words[2] = pc
    return {"pc": pc, "frame_pointer": CALLER_FP,
            "source": source, "cache_index": -1 if source == "ordinary_ram" else 3,
            "words": words}


def block(name, address, words):
    return {"name": name, "address": address, "words": list(words)}


def contact_events(accepted=True, source="ordinary_ram"):
    """Original test data: one ordered neighborhood and one 17-word reply."""
    events = []
    index = 17 if accepted else -1
    for order, kind in enumerate(("query_pre", "query_count", "query_post"), 1):
        regs, obj, globals_, cursor = [0] * 32, [0] * 256, [0] * 192, [0] * 16
        regs[0], regs[2], regs[31] = CALLER_FP, 0x5cd898, HELPER_FP
        regs[16:20] = [2, index & 0xffffffff, POINT_ADDRESS, 0]
        regs[23], obj[34], globals_[149] = 300, index & 65535, 2
        cursor[4] = 0x918000 + (0 if order == 1 else 4)
        if kind == "query_post":
            regs[31] = CALLER_FP
            if accepted:
                regs[16], regs[17], regs[29] = 16, 68, 18
                cursor[4] += 36
        events.append({"schema": 1, "event": kind, "pc": (0x5c9bc0, 0x5c9c08, 0x5cd898)[order - 1],
                       "call_id": 7, "object": 0x210000, "frame": 100, "instructions": order,
                       "registers": regs, "object_words": obj,
                       "return_frame": saved_frame(0x5cd898, source),
                       "blocks": [block("globals", 0x214100, globals_),
                                  block("query_stage_global", 0x20a280, cursor),
                                  block("query_descriptors", 0x5dcac0, [0] * 24),
                                  block("query_point", POINT_ADDRESS, POINT)]})
    for order, kind in enumerate(("contact_pre", "contact_reply", "contact_post"), 1):
        regs = [0] * 32
        regs[0], regs[2], regs[31] = CALLER_FP, 0x5c9cc0, HELPER_FP
        regs[16], regs[17], regs[21] = POINT_ADDRESS, OUTPUT_ADDRESS, PAYLOAD[-1]
        output = list(range(10, 18))
        if kind == "contact_post":
            regs[31], regs[16], regs[17] = CALLER_FP, 0xdeadbeef, 0xcafebabe
            output[3:] = list(range(30, 35))
        blocks = [block("contact_point", POINT_ADDRESS, POINT),
                  block("contact_output", OUTPUT_ADDRESS, output)]
        if kind == "contact_reply":
            blocks += [block("contact_reply_prefix", HELPER_FP + 0x60, PAYLOAD[:12]),
                       block("contact_reply_tail", HELPER_FP + 0x50, PAYLOAD[12:16])]
        events.append({"schema": 1, "event": kind, "pc": (0x5c9cc0, 0x5c9dac, 0x5cd8a8)[order - 1],
                       "call_id": 7, "object": 0x210000, "frame": 100, "instructions": order * 10,
                       "registers": regs, "return_frame": saved_frame(0x5cd8a8, source), "blocks": blocks})
    return events


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_capture(parent, events=None, marker_changes=None, validation_changes=None):
    capture = parent / "capture"
    capture.mkdir(exist_ok=True)
    events = contact_events() if events is None else events
    stream, marker = capture / "events.jsonl", capture / "capture.json"
    stream.write_text("".join(json.dumps(event) + "\n" for event in events))
    metadata = {"schema": 1, "complete": True, "calls": 1, "completed_calls": 1, "events": len(events)}
    metadata.update(marker_changes or {})
    marker.write_text(json.dumps(metadata))
    validation = {"status": "passed", "exact_checkpoint_match": True,
                  "capture_files_sha256": {"capture.json": sha(marker), "events.jsonl": sha(stream)}}
    validation.update(validation_changes or {})
    (parent / "validation.json").write_text(json.dumps(validation))
    return capture


class ContactQueryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)

    def test_reply_order_saved_return_and_changed_argument_registers(self):
        for source in ("ordinary_ram", "register_cache"):
            with self.subTest(source=source):
                events = contact_events(source=source)
                rows, provenance = CHECKER.contact_records(events)
                self.assertEqual(rows, [(2, 17, 0, *POINT, *PAYLOAD)])
                self.assertEqual(provenance, [{"call_id": 7, "object": 0x210000, "frame": 100,
                                              "helper_return": 0x5cd8a8, "instructions": 20}])
                self.assertNotEqual(events[3]["registers"][2], provenance[0]["helper_return"])

    def test_incomplete_duplicate_reordered_and_rejected_queries(self):
        events = contact_events()
        bad_streams = [[], events[:3], events[3:], events[:-1], events[:4] + events[5:],
                       events[:4] + [events[3]] + events[4:], events[:5] + [events[4], events[5]],
                       events[:3] + [events[4], events[3], events[5]], events + events,
                       contact_events(accepted=False)]
        for bad in bad_streams:
            with self.subTest(events=len(bad)), self.assertRaises(ValueError):
                CHECKER.contact_records(bad)

    def test_mismatched_identity_reply_buffers_and_preserved_prefix(self):
        mutations = [
            lambda e: e[3].__setitem__("call_id", 8),
            lambda e: e[4].__setitem__("object", 0x220000),
            lambda e: e[4].__setitem__("frame", 99),
            lambda e: e[5].__setitem__("instructions", 20),
            lambda e: e[3].__setitem__("schema", 2),
            lambda e: e[4].__setitem__("pc", 0x5c9da8),
            lambda e: e[5].__setitem__("pc", 0x5cd8ac),
            lambda e: e[4]["registers"].__setitem__(31, HELPER_FP + 64),
            lambda e: e[5]["registers"].__setitem__(31, CALLER_FP + 64),
            lambda e: e[3]["registers"].__setitem__(18, 1),
            lambda e: e[3]["registers"].__setitem__(16, POINT_ADDRESS + 4),
            lambda e: e[3]["return_frame"]["words"].__setitem__(2, 0x5cd8ac),
            lambda e: e[3]["return_frame"].__setitem__("source", "device"),
            lambda e: e[4]["blocks"][0]["words"].__setitem__(0, 0),
            lambda e: e[4]["blocks"][1]["words"].__setitem__(7, 99),
            lambda e: e[5]["blocks"][1]["words"].__setitem__(0, 99),
            lambda e: e[4]["blocks"][2]["words"].pop(),
            lambda e: e[4]["blocks"][3]["words"].append(0),
            lambda e: e[4]["blocks"].append(copy.deepcopy(e[4]["blocks"][2])),
            lambda e: e[4]["registers"].__setitem__(21, True),
        ]
        for index, mutate in enumerate(mutations):
            events = contact_events()
            mutate(events)
            with self.subTest(mutation=index), self.assertRaises(ValueError):
                CHECKER.contact_records(events)

    def test_query_must_complete_before_its_contact_helper_starts(self):
        events = contact_events()
        events[2]["instructions"] = 100
        with self.assertRaises(ValueError):
            CHECKER.contact_records(events)

    def test_reader_binds_completed_marker_stream_and_validation(self):
        with tempfile.TemporaryDirectory(prefix="contact-query-proof-", dir=ROOT / "build") as directory:
            parent = Path(directory)
            capture = write_capture(parent)
            rows, _, identities = CHECKER.read_capture(capture)
            self.assertEqual(rows, [(2, 17, 0, *POINT, *PAYLOAD)])
            self.assertEqual(len(identities), 3)
            with (capture / "events.jsonl").open("a") as stream:
                stream.write("\n")
            with self.assertRaisesRegex(ValueError, "identity differs"):
                CHECKER.read_capture(capture)
            for metadata in ({"complete": False}, {"calls": 0, "completed_calls": 0},
                             {"completed_calls": 2}, {"events": 5}):
                capture = write_capture(parent, marker_changes=metadata)
                with self.subTest(marker=metadata), self.assertRaises(ValueError):
                    CHECKER.read_capture(capture)
            for validation in ({"status": "failed"}, {"exact_checkpoint_match": False}):
                capture = write_capture(parent, validation_changes=validation)
                with self.subTest(validation=validation), self.assertRaises(ValueError):
                    CHECKER.read_capture(capture)
            capture = write_capture(parent, events=contact_events()[:-1])
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                CHECKER.read_capture(capture)
            capture = write_capture(parent)
            (capture / "capture.json").write_bytes(b" " * 16385)
            with self.assertRaisesRegex(ValueError, "reader bound"):
                CHECKER.read_capture(capture)

    def test_reader_rejects_external_and_redirected_capture_paths(self):
        with tempfile.TemporaryDirectory(prefix="contact-query-outside-") as external, \
             tempfile.TemporaryDirectory(prefix="contact-query-link-", dir=ROOT / "build") as private:
            alias = Path(private) / "alias"
            alias.symlink_to(external, target_is_directory=True)
            for path in (Path(external), alias):
                with self.subTest(path=str(path)), self.assertRaisesRegex(ValueError, "private build"):
                    CHECKER.read_capture(path)


if __name__ == "__main__":
    unittest.main()
