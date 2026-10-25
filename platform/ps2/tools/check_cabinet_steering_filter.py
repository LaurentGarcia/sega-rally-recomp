#!/usr/bin/env python3
"""Privately compare the complete cabinet steering filter with original code."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time

from check_contact_neighborhood import ROOT, PROGRAM_SHA256, digest, private_output
from check_driving_contact import saved_return

REGIONS = {
    "filter": (0x39a90, 0x39aec, "0db1b90ee70640a562c233567f7a5aa20ff30856af6edc4941500094b7d1a645"),
    "mapping": (0x395d8, 0x39668, "1363e4086b6a24a4c452c6c7b7912e51e6bf12c6ac90b6117b3ac0278b22de88"),
}
PCS = {"steering_filter_pre": 0x5d8a90, "steering_filter_post": 0x5d8ae8}


def integer(value, bits):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise ValueError("Invalid unsigned filter field")
    return value


def block(event, name, address):
    matches = [b for b in event.get("blocks", []) if isinstance(b, dict) and b.get("name") == name]
    if len(matches) != 1 or matches[0].get("address") != address or \
       not isinstance(matches[0].get("words"), list) or len(matches[0]["words"]) != 1:
        raise ValueError("Missing, duplicated or displaced filter block: " + name)
    return integer(matches[0]["words"][0], 32)


def filter_rows(events):
    pending = None
    rows, pairs, seen = [], [], set()
    last_instruction = -1
    for event in events:
        if not isinstance(event, dict) or event.get("event") not in PCS:
            raise ValueError("Unexpected filter stream event")
        kind = event["event"]
        if event.get("schema") != 1 or event.get("pc") != PCS[kind]:
            raise ValueError("Unexpected filter boundary/schema")
        pair = integer(event.get("pair_id"), 64)
        frame = integer(event.get("frame"), 64)
        instruction = integer(event.get("instructions"), 64)
        fp = integer(event.get("frame_pointer"), 32)
        ac = integer(event.get("ac"), 32)
        regs = event.get("registers")
        if not isinstance(regs, list) or len(regs) != 32:
            raise ValueError("Filter requires complete register state")
        for word in regs:
            integer(word, 32)
        if not pair or fp & 3 or regs[31] != fp or ac >> 30 or instruction <= last_instruction:
            raise ValueError("Invalid filter identity/order/rounding")
        last_instruction = instruction
        returned = event.get("return_frame")
        if not isinstance(returned, dict):
            raise ValueError("Missing actual caller frame")
        return_pc, return_fp = saved_return(event)
        if return_pc & 3:
            raise ValueError("Unaligned filter caller identity")
        history = block(event, "steering_history", 0x2142c0)
        sample = block(event, "steering_input", 0x202050) & 255
        if kind == "steering_filter_pre":
            if pending is not None or pair in seen or len(rows) >= 16384:
                raise ValueError("Duplicate, nested or excessive filter input")
            pending = {"pair": pair, "frame": frame, "instruction": instruction, "fp": fp,
                       "return_pc": return_pc, "return_fp": return_fp, "proof": returned,
                       "history": history, "sample": sample}
            continue
        if pending is None or pair != pending["pair"] or frame < pending["frame"] or fp != pending["fp"] or \
           return_pc != pending["return_pc"] or return_fp != pending["return_fp"] or \
           returned != pending["proof"] or sample != pending["sample"]:
            raise ValueError("Unpaired filter boundary or changed input/caller identity")
        if regs[16] != history ^ 0x80000000:
            raise ValueError("Captured filter return is not sign-flipped history")
        rows.append((pending["history"], sample, history, regs[16]))
        pairs.append({"pair_id": pair, "entry_frame": pending["frame"], "exit_frame": frame,
                      "instructions": instruction - pending["instruction"], "return_pc": return_pc})
        seen.add(pair)
        pending = None
    if pending is not None or not rows:
        raise ValueError("Incomplete or empty filter capture")
    return rows, pairs


def validate_capture_counts(marker, row_count):
    entries = integer(marker.get("steering_filter_entries"), 64)
    completions = integer(marker.get("steering_filter_completions"), 64)
    samples = integer(marker.get("steering_filter_samples"), 64)
    if not entries or entries != completions or not samples or samples > entries or samples != row_count:
        raise ValueError("Filter capture counts differ from complete paired stream")


def capture_rows(directory):
    directory = private_output(Path(directory))
    paths = [directory / "capture.json", directory / "steering-filter.jsonl", directory.parent / "validation.json"]
    for path, limit in zip(paths, (16384, 32*1024*1024, 1024*1024)):
        if path.stat().st_size > limit:
            raise ValueError("Filter capture exceeds reader bound")
    hashes = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("complete") is not True or marker.get("schema") != 1 or \
       validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Filter capture lacks completed observer-parity validation")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Filter capture differs from validation identity")
    rows, pairs = filter_rows([json.loads(line) for line in paths[1].read_text().splitlines()])
    validate_capture_counts(marker, len(rows))
    return rows, pairs, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    image = program.read_bytes()
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        parser.error("Program is not the verified Revision B image")
    for name, (start, end, expected) in REGIONS.items():
        if hashlib.sha256(image[start:end]).hexdigest() != expected:
            parser.error("Unexpected original region: " + name)
    rows, pairs, capture_hashes = capture_rows(args.capture) if args.capture else ([], [], {})
    output = private_output(args.output)
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = ["platform/ps2/tests/cabinet_steering_filter_oracle.cpp",
        "platform/ps2/native/cabinet_steering_filter.cpp", "platform/ps2/native/cabinet_steering.cpp",
        "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    dependencies = sources + ["platform/ps2/native/cabinet_steering_filter.h",
        "platform/ps2/native/cabinet_steering.h", "platform/ps2/tests/cabinet_steering_filter_cases.h",
        "src/runtime/cpu.h", "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
        "platform/ps2/tools/check_cabinet_steering_filter.py", "platform/ps2/tools/check_contact_neighborhood.py",
        "platform/ps2/tools/check_driving_contact.py"]
    hashes = {p: digest(ROOT / p) for p in dependencies}
    executable = output / "steering-filter-oracle"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
        *(str(ROOT / p) for p in sources), "-o", str(executable)]
    build = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(build.stdout + build.stderr)
    build.check_returncode()
    run = [str(executable), str(program)]
    records_hash = None
    if rows:
        records = output / "filter-records.bin"
        with records.open("xb") as stream:
            stream.write(b"STFILT1\0" + struct.pack("<I", len(rows)))
            for row in rows:
                stream.write(struct.pack("<4I", *row))
        records_hash = digest(records)
        run.append(str(records))
    started = time.monotonic()
    tested = subprocess.run(run, capture_output=True, text=True, timeout=60)
    elapsed = time.monotonic() - started
    (output / "oracle.log").write_text(tested.stdout + tested.stderr)
    if hashes != {p: digest(ROOT / p) for p in dependencies} or digest(program) != PROGRAM_SHA256 or \
       capture_hashes != {p: digest(Path(p)) for p in capture_hashes}:
        raise RuntimeError("Steering filter evidence changed during comparison")
    report = {"schema": 1, "status": "pass" if tested.returncode == 0 else "failed",
        "scope": "Complete bounded cabinet filter; no vehicle simulation or calibration",
        "program_sha256": PROGRAM_SHA256, "regions": REGIONS, "sources_sha256": hashes,
        "compiler_command": command, "executable_sha256": digest(executable),
        "result": tested.stdout.strip(), "exit_code": tested.returncode, "elapsed_seconds": round(elapsed, 3),
        "capture_sha256": capture_hashes, "records_sha256": records_hash, "live_records": len(rows),
        "live_unique_inputs": len({row[:2] for row in rows}), "live_pairs": pairs,
        "exhaustive_history_domain": False, "exception_status_verified": False,
        "race_integration_verified": False, "physical_ps2_verified": False}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    if tested.returncode:
        print(tested.stderr, end="")
    tested.check_returncode()


if __name__ == "__main__":
    main()
