"""Original contact-result arithmetic and strict private-observation parsing."""
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
    import check_contact_result as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_query import contact_events, block, POINT, PAYLOAD, CALLER_FP, HELPER_FP


def alternate():
    events = contact_events()
    for event in events:
        event["ac"] = 0x3f001000
    marker = {"schema": 1, "complete": True, "calls": 1, "completed_calls": 1,
              "detailed_calls": 1, "contact_entries": 1, "contact_completions": 1, "events": len(events)}
    return events, marker


def main_pass():
    """Synthetic four-slot pipeline, including response after the NEXT query."""
    obj, fp, caller = 0x210000, CALLER_FP, CALLER_FP-0x100
    proof = {"pc": 0x5d421c, "frame_pointer": caller, "source": "ordinary_ram", "cache_index": -1, "words": [0]*16}
    proof["words"][2] = proof["pc"]
    events, queries, contacts = [], [], []
    for index in range(4):
        group, _ = alternate()
        point, output = fp+0x40+12*index, 0x2138d0+36*index
        query_return = 0x5d36dc if index == 0 else 0x5d3728
        for event in group:
            kind, regs = event["event"], event["registers"]
            event.update(main_frame_pointer=fp, parent_return_frame=copy.deepcopy(proof), fp_bits=[0]*4)
            event.setdefault("object_words", [0]*256)[34] = 17
            if kind.startswith("query_"):
                event["query_index"] = index
                event["return_frame"]["pc"] = query_return
                event["return_frame"]["words"][2] = query_return
                regs[2] = 0x5c9bc0  # Child RIP changed by an interrupt; proof remains authoritative.
                if kind == "query_pre": regs[18] = point
                if kind == "query_post": event["pc"] = query_return
                event["blocks"] = [b for b in event["blocks"] if b["name"] != "globals"]
                event["blocks"].append(block("course_selector", 0x214354, [2]))
            else:
                event["contact_index"] = index
                event["return_frame"]["pc"] = 0x5d3710
                event["return_frame"]["words"][2] = 0x5d3710
                if kind == "contact_pre": regs[16], regs[17] = point, output
                if kind == "contact_post": event["pc"] = 0x5d3710
            for item in event["blocks"]:
                if item["name"] in ("query_point", "contact_point"): item["address"] = point
                if item["name"] == "contact_output": item["address"] = output
        queries.append(group[:3]);contacts.append(group[3:])
    def boundary(kind, pc, index=None):
        regs, words = [0]*32, [0]*256
        regs[0], regs[13], regs[16], regs[17], regs[18], regs[31] = caller, obj, obj, 0x220000, 0x220020, fp
        if kind == "main_return": regs[31] = caller
        if kind == "response_post": regs[5] = index
        record = {"schema": 1, "event": kind, "call_id": 7, "object": obj, "main_frame_pointer": fp,
                  "frame": 100, "pc": pc, "ac": 0x3f001000, "registers": regs, "fp_bits": [0]*4,
                  "object_words": words, "return_frame": copy.deepcopy(proof),
                  "parent_return_frame": copy.deepcopy(proof), "blocks": []}
        if index is not None: record["contact_index"] = index
        events.append(record)
    boundary("main_entry", 0x5d3380)
    boundary("main_prequery", 0x5d36d8)
    events.extend(queries[0])
    for index in range(4):
        events.extend(contacts[index])
        if index < 3: events.extend(queries[index+1])
        boundary("response_post", 0x5d3a30, index)
    boundary("main_return", 0x5d421c)
    for index, event in enumerate(events): event["instructions"] = 10*(index+1)
    marker = {"schema": 1, "complete": True, "main_calls": 1, "main_completions": 1,
              "main_samples": 1, "main_events": len(events), "main_queries": 4,
              "main_query_completions": 4, "main_contacts": 4, "main_contact_completions": 4,
              "main_cross_pairs": 0, "main_sampled_cross_pairs": 0}
    return events, marker


def private_capture(parent, stream="events", metadata=None, validation_fields=None):
    events, marker = main_pass() if stream == "main-contact" else alternate()
    marker.update(metadata or {})
    directory = parent / "capture"
    directory.mkdir(exist_ok=True)
    data = directory / (stream+".jsonl")
    data.write_text("".join(json.dumps(e)+"\n" for e in events))
    manifest = directory / "capture.json"
    manifest.write_text(json.dumps(marker))
    validation = {"status": "passed", "exact_checkpoint_match": True,
                  "capture_files_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (data, manifest)}}
    validation.update(validation_fields or {})
    (parent / "validation.json").write_text(json.dumps(validation))
    return directory


class ContactResultTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)

    def test_native_arithmetic_preservation_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-result-") as directory:
            executable = Path(directory) / "contact-result"
            sources = ["platform/ps2/native/contact_result.cpp", "platform/ps2/tests/contact_result_test.cpp"]
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       *(str(ROOT / source) for source in sources), "-o", str(executable)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("CONTACT_RESULT_PROBE PASS cases=2440 hash=82d33d3b52e3a7d4", tested.stdout)
            self.assertIn("CONTACT_RESULT_HOST PASS cases=40392 hash=385ddd84352f0d5e", tested.stdout)
            self.assertIn("CONTACT_RESULT_REJECTION PASS checks=34", tested.stdout)
            self.assertIn("CONTACT_RESULT_INTERPOLATION PASS cases=3", tested.stdout)

    def test_raw_reply_and_old_new_tail_order_for_both_streams(self):
        expected = tuple(POINT+PAYLOAD+list(range(13,18))+list(range(30,35)))
        for stream, fixture, count in (("events",alternate,1),("main-contact",main_pass,4)):
            with self.subTest(stream=stream):
                events, marker = fixture()
                rows, provenance = CHECKER.result_rows(events, marker, stream)
                self.assertEqual(rows, [expected]*count)
                self.assertEqual(len(provenance), count)
                self.assertTrue(all(p["stream"] == stream for p in provenance))
                self.assertTrue(all(len(row) == 30 for row in rows))

    def test_ac_and_saved_return_proof_cannot_change_or_disappear(self):
        for stream, fixture in (("events",alternate),("main-contact",main_pass)):
            original, marker = fixture()
            contacts = [i for i,e in enumerate(original) if e["event"].startswith("contact_")]
            mutations = []
            for i in contacts[:3]:
                mutations += [lambda e,i=i:e[i].__setitem__("ac",0x7f001000),
                              lambda e,i=i:e[i].pop("ac"),
                              lambda e,i=i:e[i].pop("return_frame"),
                              lambda e,i=i:e[i]["return_frame"]["words"].__setitem__(2,0x5cd8ac)]
            mutations += [lambda e:e[contacts[0]]["return_frame"].__setitem__("source","device"),
                          lambda e:e[contacts[0]]["return_frame"].__setitem__("cache_index",True)]
            for index, mutate in enumerate(mutations):
                bad = copy.deepcopy(original);mutate(bad)
                with self.subTest(stream=stream,mutation=index), self.assertRaises(ValueError):
                    CHECKER.result_rows(bad,marker,stream)

    def test_main_pipeline_parent_slot_and_count_failures(self):
        original, marker = main_pass()
        kinds = {kind:next(i for i,e in enumerate(original) if e["event"]==kind)
                 for kind in ("query_pre","query_post","contact_pre","contact_post","response_post","main_return")}
        mutations = [
            lambda e:e.pop(kinds["response_post"]),
            lambda e:e[kinds["response_post"]].__setitem__("contact_index",3),
            lambda e:e[kinds["response_post"]].__setitem__("pc",0x5d3a34),
            lambda e:e[kinds["contact_pre"]].__setitem__("contact_index",1),
            lambda e:e[kinds["query_pre"]].__setitem__("query_index",1),
            lambda e:e[kinds["contact_post"]].__setitem__("pc",0x5cd8a8),
            lambda e:e[kinds["query_post"]].__setitem__("pc",0x5cd898),
            lambda e:e[kinds["contact_pre"]]["parent_return_frame"]["words"].__setitem__(3,1),
            lambda e:e[kinds["main_return"]]["registers"].__setitem__(31,HELPER_FP),
            lambda e:e[0]["return_frame"]["words"].__setitem__(2,0x5d4218),
        ]
        for index, mutate in enumerate(mutations):
            bad = copy.deepcopy(original);mutate(bad)
            adjusted = dict(marker, main_events=len(bad))
            with self.subTest(mutation=index), self.assertRaises(ValueError):
                CHECKER.result_rows(bad,adjusted,"main-contact")
        for key,value in (("main_calls",0),("main_completions",2),("main_samples",2),
                          ("main_events",30),("main_queries",3),("main_query_completions",3),
                          ("main_contacts",3),("main_contact_completions",3)):
            with self.subTest(field=key), self.assertRaises(ValueError):
                CHECKER.result_rows(original,dict(marker,**{key:value}),"main-contact")

    def test_main_selector_has_one_unambiguous_bounded_source(self):
        original, marker = main_pass()
        pre = next(i for i,e in enumerate(original) if e["event"]=="query_pre")
        mutations = [
            lambda e:e[pre]["blocks"].append(copy.deepcopy(e[pre]["blocks"][-1])),
            lambda e:e[pre]["blocks"].pop(),
            lambda e:e[pre]["blocks"][-1]["words"].append(0),
            lambda e:e[pre]["blocks"][-1]["words"].__setitem__(0,3),
            lambda e:e[pre]["blocks"][-1].__setitem__("address",0x214350),
            lambda e:e[pre]["blocks"].append(block("globals",0x214100,[0]*149+[2]+[0]*42)),
        ]
        for index,mutate in enumerate(mutations):
            bad=copy.deepcopy(original);mutate(bad)
            with self.subTest(mutation=index),self.assertRaises(ValueError):
                CHECKER.result_rows(bad,marker,"main-contact")

    def test_consistently_repointed_children_cannot_escape_main_parent_arrays(self):
        original, marker = main_pass()
        for mutation in ("point","output","query-caller","contact-caller"):
            events = copy.deepcopy(original)
            for event in events:
                kind = event["event"]
                query = kind.startswith("query_") and event["query_index"] == 0
                contact = kind.startswith("contact_") and event["contact_index"] == 0
                if mutation == "point" and (query or contact):
                    if kind == "query_pre": event["registers"][18] += 4
                    if kind == "contact_pre": event["registers"][16] += 4
                    for item in event["blocks"]:
                        if item["name"] in ("query_point","contact_point"): item["address"] += 4
                if mutation == "output" and contact:
                    if kind == "contact_pre": event["registers"][17] += 4
                    for item in event["blocks"]:
                        if item["name"] == "contact_output": item["address"] += 4
                if (mutation == "query-caller" and query) or (mutation == "contact-caller" and contact):
                    event["registers"][0] += 0x40
                    event["return_frame"]["frame_pointer"] += 0x40
                    if kind.endswith("_post"): event["registers"][31] += 0x40
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                CHECKER.result_rows(events,marker,"main-contact")

    def test_reader_binds_both_streams_to_completion_and_exact_hashes(self):
        with tempfile.TemporaryDirectory(prefix="contact-result-proof-",dir=ROOT/"build") as directory:
            parent=Path(directory)
            for stream in ("events","main-contact"):
                capture=private_capture(parent,stream)
                rows,_,hashes=CHECKER.read_capture(capture,stream)
                self.assertEqual(len(rows),1 if stream=="events" else 4)
                self.assertEqual(len(hashes),3)
                with (capture/(stream+".jsonl")).open("a") as output: output.write("\n")
                with self.assertRaisesRegex(ValueError,"validated identity"):
                    CHECKER.read_capture(capture,stream)
                for fields in ({"status":"failed"},{"exact_checkpoint_match":False}):
                    capture=private_capture(parent,stream,validation_fields=fields)
                    with self.assertRaises(ValueError): CHECKER.read_capture(capture,stream)
                capture=private_capture(parent,stream,metadata={"complete":False})
                with self.assertRaises(ValueError): CHECKER.read_capture(capture,stream)
            capture=private_capture(parent)
            (capture/"capture.json").write_bytes(b" "*16385)
            with self.assertRaisesRegex(ValueError,"reader bound"): CHECKER.read_capture(capture)
            with self.assertRaisesRegex(ValueError,"Unsupported"): CHECKER.read_capture(capture,"other")
        with tempfile.TemporaryDirectory(prefix="contact-result-outside-") as external, \
             tempfile.TemporaryDirectory(prefix="contact-result-alias-",dir=ROOT/"build") as private:
            alias=Path(private)/"alias";alias.symlink_to(external,target_is_directory=True)
            for path in (Path(external),alias):
                with self.assertRaisesRegex(ValueError,"real build directory"): CHECKER.read_capture(path)


if __name__ == "__main__":
    unittest.main()
