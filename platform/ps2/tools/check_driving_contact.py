#!/usr/bin/env python3
"""Compare typed contact neighborhoods against complete private query snapshots."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output

PCS = {"query_pre": 0x5c9bc0, "query_count": 0x5c9c08}
# Original direct-call return sites. A sampled call must additionally match
# its own saved return register and observed parent object's signed index.
QUERY_RETURN_PCS = frozenset(0x59f000 + offset for offset in (
    0x17df8, 0x2be4c, 0x2ca00, 0x2d2d8, 0x2e898, 0x2ef64, 0x2fc30,
    0x2fc7c, 0x346dc, 0x34728, 0x35708, 0x42a30, 0x48ef8, 0x59a00))


def integer(value, bits):
    if type(value) is not int or not 0 <= value < (1 << bits):
        raise ValueError("Invalid unsigned capture field")
    return value


def block(event, name, address, count):
    matches = [b for b in event.get("blocks", []) if isinstance(b, dict) and b.get("name") == name]
    if len(matches) != 1 or matches[0].get("address") != address:
        raise ValueError("Missing, duplicated, or displaced query block: " + name)
    words = matches[0].get("words")
    if not isinstance(words, list) or len(words) != count:
        raise ValueError("Wrong query block size: " + name)
    return [integer(value, 32) for value in words]


def query_rows(events):
    """Require pre/count/post ordering; repeated queries retain their call ordinal."""
    pending = None
    rows, provenance, ordinals = [], [], {}
    last_instruction = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("Capture event is not an object")
        kind = event.get("event")
        if kind not in ("query_pre", "query_count", "query_post"):
            continue
        valid_pc = event.get("pc") in QUERY_RETURN_PCS if kind == "query_post" else event.get("pc") == PCS[kind]
        if event.get("schema") != 1 or not valid_pc:
            raise ValueError("Wrong query event boundary/schema")
        call, obj = integer(event.get("call_id"), 64), integer(event.get("object"), 32)
        frame, instruction = integer(event.get("frame"), 64), integer(event.get("instructions"), 64)
        if not call or obj & 3 or instruction <= last_instruction:
            raise ValueError("Query event identity/order is invalid")
        last_instruction = instruction
        regs, words = event.get("registers"), event.get("object_words")
        if not isinstance(regs, list) or len(regs) != 32 or not isinstance(words, list) or len(words) != 256:
            raise ValueError("Query requires complete registers/object snapshot")
        for value in regs + words:
            integer(value, 32)
        cursor = block(event, "query_stage_global", 0x20a280, 16)[4]
        if cursor & 3:
            raise ValueError("Unaligned query staging cursor")
        if kind == "query_pre":
            if pending is not None or len(rows) >= 16384:
                raise ValueError("Nested or excessive query input")
            selector = regs[16]
            if regs[2] not in QUERY_RETURN_PCS:
                raise ValueError("Query saved return address is not a verified direct caller")
            if selector >= 8 or block(event, "globals", 0x214100, 192)[149] != selector:
                raise ValueError("Query selector differs from verified descriptor domain/global")
            block(event, "query_descriptors", 0x5dcac0, 24)
            block(event, "query_point", regs[18], 3)
            index = words[34] & 65535
            signed = index if index < 32768 else index - 65536
            if regs[17] != (signed & 0xffffffff):
                raise ValueError("Query index differs from signed object halfword")
            pending = {"call": call, "object": obj, "frame": frame, "instruction": instruction,
                       "cursor": cursor, "selector": selector, "index": index, "regs": regs}
            continue
        if pending is None or pending["call"] != call or pending["object"] != obj or frame < pending["frame"]:
            raise ValueError("Unpaired or mismatched query boundary")
        if kind == "query_count":
            if "count" in pending or cursor != pending["cursor"] + 4 or regs[17] != pending["regs"][17]:
                raise ValueError("Query count boundary differs from its input")
            pending["count"] = regs[23]
            if not 1 <= pending["count"] <= 65535:
                raise ValueError("Live query count is outside native supported domain")
            continue
        if event["pc"] != pending["regs"][2]:
            raise ValueError("Query returned to a different caller than its saved return address")
        if "count" not in pending or regs[23] != pending["count"]:
            raise ValueError("Query exit lacks its unchanged count")
        advance = cursor - pending["cursor"]
        if advance not in (4, 40):
            raise ValueError("Query exit has an unexpected staging extent")
        accepted = advance == 40
        previous, current, following = (regs[16], regs[17] // 4, regs[29]) if accepted else (0, 0, 0)
        if accepted and (regs[17] % 4 or max(previous, current, following) > 65535 or
                         regs[19] != ((pending["regs"][19] << 1) & 0xffffffff)):
            raise ValueError("Query exit output encoding is unsupported")
        ordinal = ordinals.get(call, 0)
        ordinals[call] = ordinal + 1
        rows.append((pending["count"], pending["index"], int(accepted), previous, current, following))
        provenance.append({"call_id": call, "query_ordinal": ordinal, "object": obj,
                           "entry_frame": pending["frame"], "exit_frame": frame,
                           "entry_instructions": pending["instruction"], "exit_instructions": instruction,
                           "selector": pending["selector"], "return_pc": event["pc"]})
        pending = None
    if pending is not None or not rows:
        raise ValueError("Incomplete or empty query capture")
    return rows, provenance


def check(capture, output):
    capture, output = Path(capture).resolve(), private_output(Path(output))
    if not capture.is_relative_to(ROOT / "build"):
        raise ValueError("Capture must remain beneath the private build directory")
    paths = [capture / "capture.json", capture / "events.jsonl", capture.parent / "validation.json"]
    for path, bound in zip(paths, (16384, 64 * 1024 * 1024, 1024 * 1024)):
        if path.stat().st_size > bound:
            raise ValueError("Capture file exceeds reader bound")
    initial = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("schema") != 1 or marker.get("complete") is not True or not marker.get("calls") or \
       marker.get("calls") != marker.get("completed_calls"):
        raise ValueError("Capture has no complete integrator boundary marker")
    if validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Capture observer lacks successful checkpoint parity validation")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != initial[str(path)]:
            raise ValueError("Capture file differs from parity validation identity")
    events = [json.loads(line) for line in paths[1].read_text().splitlines()]
    if len(events) != marker.get("events"):
        raise ValueError("Event count differs from complete marker")
    rows, provenance = query_rows(events)
    sources = ["platform/ps2/tests/contact_capture_check.cpp", "platform/ps2/native/contact_neighborhood.cpp"]
    dependencies = sources + ["platform/ps2/native/contact_neighborhood.h",
                              "platform/ps2/tools/check_driving_contact.py",
                              "platform/ps2/tools/check_contact_neighborhood.py"]
    hashes = {source: digest(ROOT / source) for source in dependencies}
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        raise ValueError("C++20 compiler required")
    output.mkdir(parents=True, exist_ok=False)
    replay, executable = output / "contact-records.bin", output / "contact-capture-check"
    with replay.open("xb") as stream:
        stream.write(b"CONTACT1" + struct.pack("<I", len(rows)))
        for row in rows:
            stream.write(struct.pack("<6I", *row))
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "platform/ps2/native"), *(str(ROOT / p) for p in sources), "-o", str(executable)]
    built = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(built.stdout + built.stderr)
    built.check_returncode()
    tested = subprocess.run([str(executable), str(replay)], capture_output=True, text=True, timeout=30)
    (output / "comparison.log").write_text(tested.stdout + tested.stderr)
    if initial != {str(path): digest(path) for path in paths} or hashes != {p: digest(ROOT / p) for p in dependencies}:
        raise RuntimeError("Capture or comparison source changed during validation")
    report = {"schema": 1, "status": "pass" if tested.returncode == 0 else "failed",
              "scope": "Captured query preparation only; no intersection or force calculations",
              "capture_sha256": initial, "sources_sha256": hashes, "compiler_command": command,
              "executable_sha256": digest(executable), "records_sha256": digest(replay),
              "result": tested.stdout.strip(), "exit_code": tested.returncode, "queries": provenance,
              "observed_counts": sorted(set(row[0] for row in rows)),
              "observed_selectors": sorted(set(row["selector"] for row in provenance)),
              "observed_signed_index_min": min(row[1] if row[1] < 32768 else row[1] - 65536 for row in rows),
              "observed_signed_index_max": max(row[1] if row[1] < 32768 else row[1] - 65536 for row in rows),
              "reference": "Captured translated host machine; not independent PCB/full MAME proof",
              "full_caller_domain_verified": False, "intersection_math_verified": False,
              "race_integration_verified": False, "physical_ps2_verified": False}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    tested.check_returncode()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(args.capture, args.output)


if __name__ == "__main__":
    main()
