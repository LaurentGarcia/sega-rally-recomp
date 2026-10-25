#!/usr/bin/env python3
"""Private original-program and captured-state check for alternate orientation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PROGRAM_SHA256 = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
REGIONS = {
    "alternate": (0x2ecb4, 0x2ee00, "849cc9ef5df8b5544b81463ad002ae6ed4c6529e69d743aff8c1a286e16be5f8"),
    "fold": (0x27b28, 0x27bc4, "bd67bceb1abed34efad3c73b3b3ac663d5bc5119dee5e58ab870c7a68cc80ed2"),
    "difference": (0x27bd8, 0x27c60, "2123ca58d261efacb63000cb8cb454a67b6394be4e84fb36f06b243bd73460c7"),
}
PCS = {"alternate_orientation_pre": 0x5cdcb4,
       "alternate_orientation_folded": 0x5cdce0,
       "alternate_orientation_post": 0x5cde00}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def private_path(path):
    root = ROOT / "build"
    resolved = Path(path).resolve()
    if root.is_symlink() or resolved == root or not resolved.is_relative_to(root):
        raise ValueError("Private evidence must stay beneath this checkout's real build directory")
    return resolved


def integer(value, bits):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise ValueError("Invalid unsigned alternate snapshot field")
    return value


def candidate_block(event, address):
    matches = [b for b in event.get("blocks", []) if isinstance(b, dict) and b.get("name") == "orientation_candidate"]
    if len(matches) != 1 or matches[0].get("address") != address:
        raise ValueError("Missing, duplicated or displaced orientation candidate")
    words = matches[0].get("words")
    if not isinstance(words, list) or len(words) != 4:
        raise ValueError("Orientation candidate requires four words")
    return [integer(word, 32) for word in words]


def orientation_rows(events):
    pending = None
    rows, provenance, seen = [], [], set()
    last_instruction = -1
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("Capture event must be an object")
        kind = event.get("event")
        if kind not in PCS:
            continue
        if event.get("schema") != 1 or event.get("pc") != PCS[kind]:
            raise ValueError("Unexpected alternate boundary/schema")
        call = integer(event.get("call_id"), 64)
        obj = integer(event.get("object"), 32)
        frame = integer(event.get("frame"), 64)
        instruction = integer(event.get("instructions"), 64)
        ac = integer(event.get("ac"), 32)
        if not call or obj & 3 or instruction <= last_instruction or ac >> 30:
            raise ValueError("Invalid alternate identity/order/rounding")
        last_instruction = instruction
        regs, words = event.get("registers"), event.get("object_words")
        if not isinstance(regs, list) or len(regs) != 32 or not isinstance(words, list) or len(words) != 256:
            raise ValueError("Alternate capture requires full registers/object window")
        for word in regs + words:
            integer(word, 32)
        address = regs[31] + 0x90
        if regs[12] != obj or regs[5] != address or address & 3 or address > 0xfffffff0:
            raise ValueError("Alternate object/candidate register mapping differs")
        candidate = candidate_block(event, address)
        if kind == "alternate_orientation_pre":
            if pending is not None or call in seen or len(rows) >= 16384:
                raise ValueError("Duplicate, nested or excessive alternate input")
            pending = {"call": call, "object": obj, "frame": frame, "instruction": instruction,
                       "address": address, "candidate": candidate, "words": words, "ac": ac}
            continue
        if pending is None or call != pending["call"] or obj != pending["object"] or \
           address != pending["address"] or frame < pending["frame"] or candidate[3] != pending["candidate"][3]:
            raise ValueError("Unpaired alternate boundary or changed identity/padding")
        if kind == "alternate_orientation_folded":
            if "folded" in pending or words != pending["words"]:
                raise ValueError("Duplicate fold boundary or unexpected object write")
            pending["folded"] = candidate
            continue
        if "folded" not in pending or candidate != pending["folded"]:
            raise ValueError("Missing folded boundary or modified folded candidate")
        if words[:14] + words[17:] != pending["words"][:14] + pending["words"][17:]:
            raise ValueError("Alternate output changed unrelated object words")
        rows.append(tuple(pending["words"][14:17] + pending["candidate"][:3] + candidate[:3] + words[14:17]))
        provenance.append({"call_id": call, "object": obj, "entry_frame": pending["frame"],
                           "exit_frame": frame, "instructions": instruction - pending["instruction"],
                           "entry_ac": pending["ac"], "exit_ac": ac})
        seen.add(call)
        pending = None
    if pending is not None or not rows:
        raise ValueError("Incomplete or empty alternate capture")
    return rows, provenance


def capture_rows(directory):
    directory = private_path(directory)
    paths = [directory / "capture.json", directory / "events.jsonl", directory.parent / "validation.json"]
    for path, limit in zip(paths, (16384, 64 * 1024 * 1024, 1024 * 1024)):
        if path.stat().st_size > limit:
            raise ValueError("Capture file exceeds reader bound")
    hashes = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("schema") != 1 or marker.get("complete") is not True or not marker.get("calls") or \
       marker.get("calls") != marker.get("completed_calls"):
        raise ValueError("Capture lacks completed calls")
    if validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Capture lacks exact observer-parity validation")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Capture identity differs from validation")
    events = [json.loads(line) for line in paths[1].read_text().splitlines()]
    if len(events) != marker.get("events"):
        raise ValueError("Capture event count differs from completed marker")
    rows, provenance = orientation_rows(events)
    return rows, provenance, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    image = program.read_bytes()
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        parser.error("Program does not match the verified Revision B image")
    for name, (start, end, expected) in REGIONS.items():
        if hashlib.sha256(image[start:end]).hexdigest() != expected:
            parser.error("Unexpected original region identity: " + name)
    rows, provenance, capture_hashes = capture_rows(args.capture) if args.capture else ([], [], {})
    output = private_path(args.output)
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = ["platform/ps2/tests/alternate_orientation_oracle.cpp",
               "platform/ps2/native/alternate_orientation.cpp", "platform/ps2/native/angle_fold.cpp",
               "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    dependencies = sources + ["platform/ps2/native/alternate_orientation.h", "platform/ps2/native/orientation.h",
        "platform/ps2/native/angle_fold.h", "platform/ps2/tests/alternate_orientation_cases.h",
        "src/runtime/cpu.h", "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
        "platform/ps2/tools/check_alternate_orientation.py"]
    hashes = {path: digest(ROOT / path) for path in dependencies}
    executable = output / "alternate-oracle"
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra",
               "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src"),
               "-I", str(ROOT / "platform/ps2/native"), *(str(ROOT / p) for p in sources), "-o", str(executable)]
    build = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(build.stdout + build.stderr)
    build.check_returncode()
    run = [str(executable), str(program)]
    replay_hash = None
    if rows:
        replay = output / "alternate-records.bin"
        with replay.open("xb") as stream:
            stream.write(b"ALTORI1\0" + struct.pack("<I", len(rows)))
            for row in rows:
                stream.write(struct.pack("<12I", *row))
        replay_hash = digest(replay)
        run.append(str(replay))
    started = time.monotonic()
    tested = subprocess.run(run, capture_output=True, text=True, timeout=60)
    elapsed = time.monotonic() - started
    (output / "oracle.log").write_text(tested.stdout + tested.stderr)
    if hashes != {path: digest(ROOT / path) for path in dependencies} or digest(program) != PROGRAM_SHA256 or \
       capture_hashes != {path: digest(Path(path)) for path in capture_hashes}:
        raise RuntimeError("Validation inputs changed during comparison")
    report = {"schema": 1, "status": "pass" if tested.returncode == 0 else "failed",
              "scope": "Bounded alternate orientation only; no car integration or TGP queries",
              "program_sha256": PROGRAM_SHA256, "regions": REGIONS, "sources_sha256": hashes,
              "capture_sha256": capture_hashes, "live_records": len(rows), "live_pairs": provenance,
              "live_unique_inputs": len({row[:6] for row in rows}), "records_sha256": replay_hash,
              "compiler_command": command, "executable_sha256": digest(executable),
              "exit_code": tested.returncode, "result": tested.stdout.strip(),
              "elapsed_seconds": round(elapsed, 3), "exhaustive_domain": False,
              "exception_status_verified": False, "race_integration_verified": False,
              "physical_ps2_verified": False,
              "reference": "Original-instruction host reference and optional translated host capture; not PCB proof"}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    if tested.returncode:
        print(tested.stderr, end="")
    tested.check_returncode()


if __name__ == "__main__":
    main()
