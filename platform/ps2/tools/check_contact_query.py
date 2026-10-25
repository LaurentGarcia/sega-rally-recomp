#!/usr/bin/env python3
"""Check complete live contact replies with private firmware and typed native search."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_count import IDENTITIES
from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block, integer, query_rows, saved_return


def contact_records(events):
    # Validate every complete neighborhood query before attaching a result helper.
    query_values, query_provenance = query_rows(events)
    accepted_queries = {p["entry_instructions"]: bool(v[2]) for p, v in zip(query_provenance, query_values)}
    query_complete = False
    pending_query = None
    pending = None
    rows, provenance = [], []
    last_instruction = -1
    for event in events:
        kind = event.get("event")
        if kind in ("query_pre", "query_count", "query_post", "contact_pre", "contact_reply", "contact_post"):
            instruction = integer(event.get("instructions"), 64)
            if instruction <= last_instruction:
                raise ValueError("Query and contact boundaries are not globally ordered")
            last_instruction = instruction
        if kind == "query_pre":
            pending_query = event
            query_complete = False
            continue
        if kind == "query_post":
            query_complete = True
            continue
        if kind not in ("contact_pre", "contact_reply", "contact_post"):
            continue
        regs = event.get("registers")
        if event.get("schema") != 1 or not isinstance(regs, list) or len(regs) != 32:
            raise ValueError("Invalid contact boundary/register schema")
        for value in regs:
            integer(value, 32)
        call, obj = integer(event.get("call_id"), 64), integer(event.get("object"), 32)
        instruction, frame = integer(event.get("instructions"), 64), integer(event.get("frame"), 64)
        if not call or obj & 3:
            raise ValueError("Invalid contact identity/order")
        if kind == "contact_pre":
            if pending or pending_query is None or not query_complete or len(rows) >= 4096 or event.get("pc") != 0x5c9cc0:
                raise ValueError("Missing query, nested helper, wrong entry or excessive contacts")
            query = pending_query
            if not accepted_queries[query["instructions"]]:
                raise ValueError("Contact result follows a rejected neighborhood query")
            if query["call_id"] != call or query["object"] != obj or query["instructions"] >= instruction:
                raise ValueError("Contact belongs to a different query")
            point = block(query, "query_point", query["registers"][18], 3)
            if regs[16] != query["registers"][18] or block(event, "contact_point", regs[16], 3) != point:
                raise ValueError("Contact point differs from staged query")
            if query["registers"][19] != 0 or regs[18] != 0:
                raise ValueError("Only the observed result slot zero is validated")
            return_pc, caller_fp = saved_return(event)
            if not return_pc or regs[16] & 3 or regs[17] & 3:
                raise ValueError("Invalid contact return/pointers")
            original = block(event, "contact_output", regs[17], 8)
            pending = {"call": call, "object": obj, "frame": frame, "instruction": instruction,
                       "point": regs[16], "output": regs[17], "point_words": point,
                       "return_pc": return_pc, "caller_fp": caller_fp if caller_fp is not None else regs[0] & ~63, "helper_fp": regs[31],
                       "original": original, "query": query}
            pending_query = None
            continue
        if pending is None or call != pending["call"] or obj != pending["object"] or frame < pending["frame"]:
            raise ValueError("Unpaired contact boundary")
        if block(event, "contact_point", pending["point"], 3) != pending["point_words"]:
            raise ValueError("Query point changed during helper")
        output = block(event, "contact_output", pending["output"], 8)
        if kind == "contact_reply":
            if event.get("pc") != 0x5c9dac or regs[31] != pending["helper_fp"] or "reply" in pending:
                raise ValueError("Duplicate or misplaced contact reply")
            if output != pending["original"]:
                raise ValueError("Output changed before reply interpretation")
            pending["reply"] = block(event, "contact_reply_prefix", regs[31] + 0x60, 12) + \
                               block(event, "contact_reply_tail", regs[31] + 0x50, 4) + [regs[21]]
        else:
            if "reply" not in pending or event.get("pc") != pending["return_pc"] or regs[31] != pending["caller_fp"]:
                raise ValueError("Contact returned without its reply or to a different caller")
            if output[:3] != pending["original"][:3]:
                raise ValueError("Contact changed the preserved output prefix")
            query = pending["query"]
            rows.append((*query["registers"][16:18], 0, *pending["point_words"], *pending["reply"]))
            provenance.append({"call_id": call, "object": obj, "frame": pending["frame"],
                               "helper_return": pending["return_pc"], "instructions": instruction - pending["instruction"]})
            pending = None
    if pending is not None or not rows:
        raise ValueError("Incomplete or empty contact capture")
    return rows, provenance


def read_capture(capture):
    capture = capture.resolve()
    if not capture.is_relative_to(ROOT / "build"):
        raise ValueError("Capture must stay beneath private build/")
    paths = [capture / "capture.json", capture / "events.jsonl", capture.parent / "validation.json"]
    for path, bound in zip(paths, (16384, 64 * 1024 * 1024, 1024 * 1024)):
        if path.stat().st_size > bound:
            raise ValueError("Capture exceeds reader bound")
    identities = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("complete") is not True or marker.get("schema") != 1 or not marker.get("calls") or marker.get("calls") != marker.get("completed_calls"):
        raise ValueError("Capture has no completed integrator evidence")
    if validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Capture has no passing observer comparison")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != identities[str(path)]:
            raise ValueError("Capture identity differs from observer comparison")
    events = [json.loads(line) for line in paths[1].read_text().splitlines()]
    if len(events) != marker.get("events"):
        raise ValueError("Capture event count mismatch")
    rows, provenance = contact_records(events)
    return rows, provenance, identities


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("program", "tgp_program", "tgp_tables", "copro_data"):
        parser.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve() for name in ("program", "tgp_program", "tgp_tables", "copro_data")}
    for name, path in inputs.items():
        if path.stat().st_size != IDENTITIES[name][0] or digest(path) != IDENTITIES[name][1]:
            parser.error(f"{name} differs from verified private image")
    rows, provenance, capture_hashes = read_capture(args.capture)
    output = private_output(args.output)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = ["platform/ps2/tests/contact_query_firmware_probe.cpp", "platform/ps2/native/contact_projection.cpp",
               "platform/ps2/native/contact_projection.h", "platform/ps2/tools/check_contact_query.py",
               "platform/ps2/native/contact_search.h", "platform/ps2/native/contact_search.cpp",
               "platform/ps2/native/contact_neighborhood.h", "platform/ps2/native/contact_neighborhood.cpp",
               "platform/ps2/tests/contact_search_cases.h",
               "platform/ps2/tools/check_contact_count.py", "platform/ps2/tools/check_driving_contact.py",
               "platform/ps2/tools/check_contact_neighborhood.py", "tools/m2tgprecomp/main.cpp",
               "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/fast_paths.h"]
    source_hashes = {p: digest(ROOT / p) for p in sources}
    output.mkdir(parents=True, exist_ok=False)
    records = output / "private-records.bin"
    records.write_bytes(b"".join(struct.pack("<23I", *row) for row in rows))
    generator, generated, executable = (output / p for p in ("m2tgprecomp", "private-tgp.cpp", "contact-query-probe"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src")]
    commands = [common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
                [str(generator), str(inputs["tgp_program"]), str(generated)],
                common + ["-DM2TGP_WITH_HOOK", "-I", str(ROOT / "platform/ps2/native"),
                          str(ROOT / sources[0]), str(ROOT / sources[1]), str(ROOT / "src/runtime/tgp.cpp"),
                          str(ROOT / "platform/ps2/native/contact_search.cpp"),
                          str(ROOT / "platform/ps2/native/contact_neighborhood.cpp"),
                          str(generated), "-o", str(executable)]]
    report = {"schema": 1, "status": "failed", "scope": "Captured raw replies, typed native selected polygon/payload and per-candidate projected predicates",
              "input_sha256": {k: IDENTITIES[k][1] for k in inputs}, "capture_sha256": capture_hashes,
              "sources_sha256": source_hashes, "records_sha256": digest(records), "queries": provenance,
              "commands": commands, "native_full_search_implemented": True, "physical_console_verified": False}
    try:
        for i, command in enumerate(commands):
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (output / f"build-{i}.log").write_text(built.stdout + built.stderr)
            built.check_returncode()
        tested = subprocess.run([str(executable), *(str(p) for p in inputs.values()), str(records), str(output / "private-replies.bin")],
                                capture_output=True, text=True, timeout=60)
        (output / "comparison.log").write_text(tested.stdout + tested.stderr)
        report.update(result=tested.stdout.strip(), exit_code=tested.returncode,
                      executable_sha256=digest(executable), generated_sha256=digest(generated))
        tested.check_returncode()
        if "CONTACT_QUERY_ORACLE PASS" not in tested.stdout:
            raise RuntimeError("Missing query comparison completion marker")
        if source_hashes != {p: digest(ROOT / p) for p in sources} or capture_hashes != {p: digest(Path(p)) for p in capture_hashes} or \
           any(digest(p) != IDENTITIES[k][1] for k, p in inputs.items()):
            raise RuntimeError("Source, input or capture changed during comparison")
        report["status"] = "passed"
        print(tested.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
