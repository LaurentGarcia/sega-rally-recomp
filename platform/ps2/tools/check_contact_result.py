#!/usr/bin/env python3
"""Compare typed contact results with private original instructions and live replies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_count import IDENTITIES
from check_contact_neighborhood import ROOT, digest, private_output
from check_contact_query import contact_records
from check_driving_contact import block, integer, saved_return

REGIONS = {
    "adapter": ("program", 0x2acc0, 0x2aec4, "4a4c446316ee2c37644b3142acd090c3c002d8933b8c8bc886e5038ca88d852e"),
    "normal": ("tgp_program", 0x30e * 4, 0x327 * 4, "4ac9c46e5516e4563babb665dbadcc80eadd1384781996c394dedf46b9daef65"),
}
SOURCES = ["platform/ps2/native/contact_result.h", "platform/ps2/native/contact_result.cpp",
    "platform/ps2/native/contact_search.h", "platform/ps2/native/contact_projection.h",
    "platform/ps2/native/contact_neighborhood.h", "platform/ps2/tests/contact_result_cases.h",
    "platform/ps2/tests/contact_result_oracle.cpp", "platform/ps2/tests/contact_search_cases.h",
    "platform/ps2/tools/check_contact_result.py", "platform/ps2/tools/check_contact_query.py",
    "platform/ps2/tools/check_contact_count.py", "platform/ps2/tools/check_contact_neighborhood.py",
    "platform/ps2/tools/check_driving_contact.py", "tools/m2tgprecomp/main.cpp",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp", "src/runtime/cpu.h", "src/runtime/cpu.cpp",
    "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/fast_paths.h"]


def ordinary(address, size):
    return not address & 3 and ((0x200000 <= address <= 0x240000-size) or
                               (0x500000 <= address <= 0x600000-size))


def main_envelopes(events, marker):
    """Require four ordered adapters inside every complete sampled main pass."""
    kinds = {"main_entry", "main_prequery", "query_pre", "query_count", "query_post",
             "contact_pre", "contact_reply", "contact_post", "response_post",
             "cross_pre", "cross_post", "main_return", "normalization_pre", "normalization_post",
             "metric_post", "step_post", "aggregate_pre", "aggregate_post"}
    parent, seen, last = None, set(), -1
    for event in events:
        if not isinstance(event, dict) or event.get("schema") != 1 or event.get("event") not in kinds:
            raise ValueError("Unsupported main-contact event/schema")
        kind = event["event"]
        call, obj = integer(event.get("call_id"), 64), integer(event.get("object"), 32)
        frame, instruction = integer(event.get("frame"), 64), integer(event.get("instructions"), 64)
        fp, pc = integer(event.get("main_frame_pointer"), 32), integer(event.get("pc"), 32)
        regs, words, floats = event.get("registers"), event.get("object_words"), event.get("fp_bits")
        if not isinstance(regs, list) or len(regs) != 32 or not isinstance(words, list) or len(words) != 256 or \
           not isinstance(floats, list) or len(floats) != 4:
            raise ValueError("Incomplete main-contact register/object snapshot")
        for value in regs + words:
            integer(value, 32)
        for value in floats:
            integer(value, 64)
        integer(event.get("ac"), 32)
        if not call or not ordinary(obj, 1024) or not ordinary(fp, 0x114) or fp & 63 or instruction <= last:
            raise ValueError("Invalid main-contact identity/range/order")
        last = instruction
        if kind == "main_entry":
            if parent is not None or call in seen or len(seen) >= 32 or pc != 0x5d3380 or regs[16] != obj or regs[31] != fp:
                raise ValueError("Invalid or nested main-contact entry")
            if not isinstance(event.get("return_frame"), dict):
                raise ValueError("Missing main saved-caller proof")
            return_pc, caller_fp = saved_return(event)
            if return_pc != 0x5d421c or event.get("parent_return_frame") != event["return_frame"]:
                raise ValueError("Main-contact parent differs from verified caller")
            parent = {"call": call, "object": obj, "fp": fp, "frame": frame,
                      "proof": event["return_frame"], "pc": return_pc, "caller_fp": caller_fp,
                      "queries": 0, "contacts": 0, "responses": 0, "prequery": False, "phase": 0}
            continue
        if parent is None or call != parent["call"] or obj != parent["object"] or fp != parent["fp"] or \
           frame < parent["frame"] or event.get("parent_return_frame") != parent["proof"]:
            raise ValueError("Main-contact event has no unchanged parent")
        if kind == "main_return":
            if parent["queries"] != 4 or parent["contacts"] != 4 or parent["responses"] != 4 or parent["phase"] or \
               pc != parent["pc"] or regs[31] != parent["caller_fp"] or event.get("return_frame") != parent["proof"]:
                raise ValueError("Incomplete main-contact pass or incorrect return")
            seen.add(call)
            parent = None
            continue
        if kind == "main_prequery":
            if parent["prequery"] or parent["queries"] or parent["phase"] or pc != 0x5d36d8 or regs[31] != fp:
                raise ValueError("Incorrect main prequery boundary")
            parent["prequery"] = True
            continue
        if kind == "response_post":
            index = integer(event.get("contact_index"), 32)
            expected_phase = 0 if index == 3 else 3
            if index != parent["responses"] or index >= 4 or parent["contacts"] != index + 1 or \
               parent["queries"] != min(index + 2, 4) or parent["phase"] != expected_phase or \
               pc != 0x5d3a30 or regs[31] != fp:
                raise ValueError("Incorrect pipelined main response boundary")
            parent["responses"] += 1
            continue
        extra_pcs = {"normalization_pre": 0x5d38ec, "normalization_post": 0x5d3968,
                     "metric_post": 0x5d3b5c, "step_post": 0x5d3b84,
                     "aggregate_pre": 0x5d3b90, "aggregate_post": 0x5d3cc0}
        if kind in extra_pcs:
            # These independently checked numerical regions share the main
            # stream. Bind their parent and legal location without claiming
            # that this adapter reader validates their numerical outputs.
            if pc != extra_pcs[kind] or regs[31] != fp or regs[13] != obj or \
               event.get("return_frame") != parent["proof"] or saved_return(event) != (parent["pc"], parent["caller_fp"]):
                raise ValueError("Additional numerical boundary has a different parent")
            if kind.startswith("normalization_"):
                index = integer(event.get("contact_index"), 32)
                if index != parent["responses"] or index >= 4 or parent["contacts"] != index + 1 or \
                   parent["queries"] != min(index + 2, 4) or parent["phase"] != (0 if index == 3 else 3):
                    raise ValueError("Normalization is outside its contact response")
            elif parent["contacts"] != 4 or parent["responses"] != 4 or parent["phase"]:
                raise ValueError("Aggregate numerical boundary precedes completed responses")
            continue
        sequence = ("query_pre", "query_count", "query_post", "contact_pre", "contact_reply", "contact_post")
        if kind in sequence:
            if not parent["prequery"] or parent["contacts"] >= 4 or kind != sequence[parent["phase"]]:
                raise ValueError("Main-contact query/result sequence differs")
            index_name = "query_index" if kind.startswith("query_") else "contact_index"
            index = integer(event.get(index_name), 32)
            if index != parent["contacts"]:
                raise ValueError("Main-contact query/result index differs")
            if kind in ("query_pre", "contact_pre"):
                if not isinstance(event.get("return_frame"), dict) or saved_return(event)[1] != fp:
                    raise ValueError("Main child has a different saved parent frame")
                parent["child_proof"] = event["return_frame"]
                if kind == "query_pre" and regs[18] != fp + 0x40 + 12 * index:
                    raise ValueError("Main query point is outside its parent's indexed point array")
                if kind == "contact_pre" and (regs[16] != fp + 0x40 + 12 * index or regs[17] != 0x2138d0 + 36 * index):
                    raise ValueError("Main adapter point/output differs from its indexed parent slots")
            elif event.get("return_frame") != parent.get("child_proof"):
                raise ValueError("Main child changed its saved caller proof")
            if kind in ("query_post", "contact_post") and regs[31] != fp:
                raise ValueError("Main child did not restore its actual parent frame")
            if kind == "query_post":
                expected = 0x5d36dc if parent["queries"] == 0 else 0x5d3728
                if pc != expected:
                    raise ValueError("Main query returned to an unexpected call site")
                parent["queries"] += 1
            if kind == "contact_post":
                if pc != 0x5d3710:
                    raise ValueError("Main result returned to an unexpected call site")
                parent["contacts"] += 1
            parent["phase"] = (parent["phase"] + 1) % 6
        elif parent["phase"]:
            raise ValueError("Unexpected event within a main query/result pair")
    calls = integer(marker.get("main_calls"), 64)
    samples = integer(marker.get("main_samples"), 64)
    if parent is not None or not calls or not samples or samples != len(seen) or samples > calls or \
       integer(marker.get("main_completions"), 64) != calls or integer(marker.get("main_events"), 64) != len(events):
        raise ValueError("Main-contact complete/sample counts differ")
    for name in ("main_queries", "main_query_completions", "main_contacts", "main_contact_completions"):
        if integer(marker.get(name), 64) != 4 * calls:
            raise ValueError("Main-contact total query/result counts differ")
    return samples * 4


def result_rows(events, marker, stream="events"):
    """Return point, raw reply, old result and observed result (30 raw words)."""
    if stream not in ("events", "main-contact"):
        raise ValueError("Unsupported contact-result stream")
    base, provenance = contact_records(events)
    if stream == "main-contact":
        expected = main_envelopes(events, marker)
    else:
        calls = integer(marker.get("calls"), 64)
        expected = integer(marker.get("detailed_calls"), 64)
        if not calls or not expected or expected > calls or integer(marker.get("completed_calls"), 64) != calls or \
           integer(marker.get("contact_entries"), 64) != calls or integer(marker.get("contact_completions"), 64) != calls or \
           integer(marker.get("events"), 64) != len(events):
            raise ValueError("Alternate contact completion/sample counts differ")
    rows, previous, offset = [], None, 0
    for event in events:
        kind = event.get("event")
        if kind in ("contact_pre", "contact_reply", "contact_post"):
            if integer(event.get("ac"), 32) >> 30:
                raise ValueError("Captured adapter does not use nearest-even rounding")
        if kind == "contact_pre":
            if not isinstance(event.get("return_frame"), dict):
                raise ValueError("Contact-result comparison requires actual saved caller proof")
            proof = event["return_frame"]
            previous = block(event, "contact_output", event["registers"][17], 8)
            address = event["registers"][17]
        elif kind == "contact_reply":
            if previous is None or event.get("return_frame") != proof:
                raise ValueError("Contact-result reply changed its saved caller proof")
        elif kind == "contact_post":
            if previous is None or offset >= len(base):
                raise ValueError("Unpaired result payload")
            if event.get("return_frame") != proof:
                raise ValueError("Contact-result return changed its saved caller proof")
            current = block(event, "contact_output", address, 8)
            rows.append(tuple(base[offset][3:]) + tuple(previous[3:]) + tuple(current[3:]))
            provenance[offset]["stream"] = stream
            previous = None
            offset += 1
    if previous is not None or len(rows) != len(base) or len(rows) != expected or any(len(row) != 30 for row in rows):
        raise ValueError("Contact-result payload/sample count differs")
    return rows, provenance


def read_capture(directory, stream="events"):
    directory = private_output(Path(directory))
    if stream not in ("events", "main-contact"):
        raise ValueError("Unsupported contact-result stream")
    paths = [directory / "capture.json", directory / (stream + ".jsonl"), directory.parent / "validation.json"]
    for path, bound in zip(paths, (16384, 64 * 1024 * 1024, 1024 * 1024)):
        if path.stat().st_size > bound:
            raise ValueError("Contact-result capture exceeds reader bound")
    hashes = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("schema") != 1 or marker.get("complete") is not True or validation.get("status") != "passed" or \
       validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Contact-result capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Contact-result capture differs from validated identity")
    rows, provenance = result_rows([json.loads(line) for line in paths[1].read_text().splitlines()], marker, stream)
    return rows, provenance, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("program", "tgp_program", "tgp_tables"):
        parser.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve() for name in ("program", "tgp_program", "tgp_tables")}
    for name, path in inputs.items():
        if path.stat().st_size != IDENTITIES[name][0] or digest(path) != IDENTITIES[name][1]:
            parser.error(name + " differs from verified private image")
    for name, (image, start, end, expected) in REGIONS.items():
        if hashlib.sha256(inputs[image].read_bytes()[start:end]).hexdigest() != expected:
            parser.error(name + " instruction region differs")
    rows, provenance, capture_hashes = [], [], {}
    if args.capture:
        for stream in ("events", "main-contact"):
            values, details, hashes = read_capture(args.capture, stream)
            rows.extend(values)
            provenance.extend(details)
            capture_hashes.update(hashes)
    output = private_output(args.output)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    source_hashes = {p: digest(ROOT / p) for p in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    generator, generated, executable = (output / p for p in ("m2tgprecomp", "private-tgp.cpp", "contact-result-oracle"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src")]
    commands = [common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
        [str(generator), str(inputs["tgp_program"]), str(generated)],
        common + ["-DM2TGP_WITH_HOOK", "-I", str(ROOT / "platform/ps2/native"),
            str(ROOT / "platform/ps2/native/contact_result.cpp"), str(ROOT / "platform/ps2/tests/contact_result_oracle.cpp"),
            str(ROOT / "src/runtime/cpu.cpp"), str(ROOT / "src/refcore/i960_ref.cpp"),
            str(ROOT / "src/runtime/tgp.cpp"), str(generated), "-o", str(executable)]]
    report = {"schema": 1, "status": "failed", "scope": "Bounded typed result adapter, including original table interpolation",
        "input_sha256": {k: IDENTITIES[k][1] for k in inputs}, "regions": REGIONS,
        "sources_sha256": source_hashes, "commands": commands, "capture_sha256": capture_hashes,
        "live_records": len(rows), "live_pairs": provenance, "live_comparison_verified": False,
        "physical_console_verified": False, "native_gameplay_integration": False}
    try:
        for i, command in enumerate(commands):
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (output / f"build-{i}.log").write_text(built.stdout + built.stderr)
            built.check_returncode()
        run = [str(executable), *(str(path) for path in inputs.values())]
        if rows:
            records = output / "private-records.bin"
            records.write_bytes(b"".join(struct.pack("<30I", *row) for row in rows))
            report["records_sha256"] = digest(records)
            run.append(str(records))
        tested = subprocess.run(run, capture_output=True, text=True, timeout=60)
        (output / "comparison.log").write_text(tested.stdout + tested.stderr)
        report.update(result=tested.stdout.strip(), exit_code=tested.returncode,
                      executable_sha256=digest(executable), generated_sha256=digest(generated), generator_sha256=digest(generator))
        tested.check_returncode()
        if "CONTACT_RESULT_ORACLE PASS" not in tested.stdout or f"live={len(rows)} " not in tested.stdout:
            raise RuntimeError("Missing contact-result comparison completion marker/count")
        if source_hashes != {p: digest(ROOT / p) for p in SOURCES} or capture_hashes != {p: digest(Path(p)) for p in capture_hashes} or \
           any(digest(p) != IDENTITIES[k][1] for k, p in inputs.items()):
            raise RuntimeError("Source, input or capture changed during comparison")
        report["status"] = "passed"
        report["live_comparison_verified"] = bool(rows)
        print(tested.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
