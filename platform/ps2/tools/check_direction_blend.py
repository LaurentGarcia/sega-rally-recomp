#!/usr/bin/env python3
"""Privately compare bounded native direction blending with the owner's program.

Optional driving captures add actual caller inputs and observed output words.
All original bytes, captures and comparison records remain below build/.
"""
from __future__ import annotations

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
HELPER_SHA256 = "494f0202ffbccc7c5f1faca495e7144f0a36e5fe3c75637767b95815473da756"
BLEND_SHA256 = "71d232c3d3f16c5230a3b3da158d3a1ad571fbfcad3de4133f0ff06a19f53670"
PCS = {"blend_pre": 0x5D010C, "blend_store": 0x5D0154, "blend_post": 0x5D0158}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unsigned(value, width, field):
    if type(value) is not int or not 0 <= value < 1 << width:
        raise ValueError("Invalid direction capture " + field)
    return value


def direction_pairs(events):
    pending, completed, rows, provenance, controls = {}, set(), [], [], set()
    for event in events:
        if not isinstance(event, dict):
            raise ValueError("Capture event must be an object")
        kind = event.get("event")
        if kind not in PCS:
            continue
        if event.get("schema") != 1 or event.get("pc") != PCS[kind]:
            raise ValueError("Wrong direction capture schema or instruction boundary")
        call = unsigned(event.get("call_id"), 32, "call_id")
        obj = unsigned(event.get("object"), 32, "object")
        unsigned(event.get("frame"), 32, "frame")
        unsigned(event.get("instructions"), 64, "instructions")
        registers, words = event.get("registers"), event.get("object_words")
        if not isinstance(registers, list) or len(registers) != 32 or not isinstance(words, list) or len(words) != 256:
            raise ValueError("Direction capture requires all registers and the bounded object")
        for value in registers + words:
            unsigned(value, 32, "word")
        if obj & 3 or registers[16] != obj or registers[17] & 3:
            raise ValueError("Direction object or output pointer mismatch")
        ac = unsigned(event.get("ac"), 32, "ac")
        if ac >> 30:
            raise ValueError("Direction capture uses unsupported rounding control")
        controls.add(ac)
        blocks = event.get("blocks")
        if not isinstance(blocks, list):
            raise ValueError("Direction capture requires memory blocks")
        output = [block for block in blocks if isinstance(block, dict) and block.get("name") == "blend_output"]
        if len(output) != 1 or output[0].get("address") != registers[17]:
            raise ValueError("Direction capture has no unique matching output block")
        output = output[0].get("words")
        if not isinstance(output, list) or len(output) != 4:
            raise ValueError("Direction output block must contain four words")
        for value in output:
            unsigned(value, 32, "output word")
        if kind == "blend_pre":
            if call in pending or call in completed or len(rows) + len(pending) >= 16384:
                raise ValueError("Duplicate or excessive direction entry")
            if registers[18] != words[0xA4 // 4] or registers[4:8] != output:
                raise ValueError("Direction input register and memory snapshots disagree")
            pending[call] = [(event, output)]
            continue
        pair = pending.get(call)
        if pair is None or call in completed or len(pair) != (1 if kind == "blend_store" else 2):
            raise ValueError("Direction events are missing, duplicated or out of order")
        before, original = pair[0]
        last, _ = pair[-1]
        if obj != before["object"] or registers[17] != before["registers"][17] or words != before["object_words"] or \
           event["frame"] < last["frame"] or event["instructions"] <= last["instructions"]:
            raise ValueError("Direction event identity, object contents or ordering differs")
        if kind == "blend_store":
            if output != original:
                raise ValueError("Direction output changed before its final store")
            pair.append((event, output))
            continue
        stored = pair[1][0]["registers"][4:8]
        if output != stored or output != registers[4:8] or output[1] != original[1] or output[3] != original[3]:
            raise ValueError("Direction final store or preserved words disagree")
        pre = before["registers"]
        rows.append((pre[4], pre[23], before["object_words"][0x9C // 4], pre[18],
                     output[0], output[2], before["frame"], call))
        provenance.append({"call_id": call, "object": obj, "frame": before["frame"],
                           "instructions": event["instructions"] - before["instructions"]})
        del pending[call]
        completed.add(call)
    if pending or not rows:
        raise ValueError("Incomplete or absent direction input/store/output triples")
    return rows, provenance, sorted(controls)


def capture_inputs(capture):
    capture = Path(capture).resolve()
    if not capture.is_relative_to(ROOT / "build"):
        raise ValueError("Driving captures must remain below the private build directory")
    marker, events, validation = capture / "capture.json", capture / "events.jsonl", capture.parent / "validation.json"
    if marker.stat().st_size > 16384 or events.stat().st_size > 64 * 1024 * 1024 or validation.stat().st_size > 1024 * 1024:
        raise ValueError("Capture input exceeds its bounded reader limit")
    hashes = {str(path): digest(path) for path in (marker, events, validation)}
    metadata, checked = json.loads(marker.read_text()), json.loads(validation.read_text())
    if metadata.get("schema") != 1 or metadata.get("complete") is not True or \
       not metadata.get("calls") or metadata.get("calls") != metadata.get("completed_calls"):
        raise ValueError("Driving capture has no complete call marker")
    if checked.get("status") != "passed" or checked.get("exact_checkpoint_match") is not True:
        raise ValueError("Driving capture has no successful observer parity validation")
    for path in (marker, events):
        if checked.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Driving capture differs from its validated identity")
    records = [json.loads(line) for line in events.read_text().splitlines()]
    if len(records) != metadata.get("events"):
        raise ValueError("Driving event count differs from its marker")
    rows, provenance, controls = direction_pairs(records)
    decoded = [[struct.unpack("<f", struct.pack("<I", row[index]))[0] for row in rows] for index in range(4)]
    summary = {"records": len(rows), "unique_inputs": len({row[:4] for row in rows}),
               "component_ranges": [{"min": min(values), "max": max(values)} for values in decoded],
               "outside_native_domain": sum(any((word & 0x7FFFFFFF) > 0x3F800000 for word in row[:4]) for row in rows),
               "observed_ac": controls, "pairs": provenance, "sha256": hashes,
               "reference": "Captured translated host machine; not independent PCB verification"}
    return rows, summary


def check(program, output, capture=None):
    program, output = Path(program).resolve(), Path(output).resolve()
    if program.stat().st_size != 2097152:
        raise ValueError("Program size differs from verified Revision B")
    image = program.read_bytes()
    if hashlib.sha256(image).hexdigest() != PROGRAM_SHA256 or \
       hashlib.sha256(image[0x30E00:0x311D8]).hexdigest() != HELPER_SHA256 or \
       hashlib.sha256(image[0x3110C:0x31158]).hexdigest() != BLEND_SHA256:
        raise ValueError("Original program/helper/blend identity mismatch")
    build = ROOT / "build"
    if build.is_symlink() or output == build or not output.is_relative_to(build):
        raise ValueError("Private evidence must be beneath this checkout's real build directory")
    rows, live = capture_inputs(capture) if capture else ([], None)
    sources = ["platform/ps2/tests/direction_blend_oracle.cpp", "platform/ps2/native/direction_blend.cpp",
               "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    dependencies = sources + ["platform/ps2/native/direction_blend.h", "platform/ps2/tests/direction_blend_cases.h",
                             "src/runtime/cpu.h", "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
                             "platform/ps2/tools/check_direction_blend.py"]
    hashes = {name: digest(ROOT / name) for name in dependencies}
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        raise ValueError("C++20 compiler required")
    output.mkdir(parents=True, exist_ok=False)
    report = {"schema": 1, "status": "validation_failed", "program_sha256": PROGRAM_SHA256,
              "helper_sha256": HELPER_SHA256, "blend_sha256": BLEND_SHA256, "sources_sha256": hashes,
              "scope": "Bounded planar blend only", "live_capture": live, "exhaustive_domain": False,
              "exception_status_verified": False, "race_integration_verified": False, "physical_ps2_verified": False}
    try:
        executable = output / "direction-oracle"
        command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                   "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
                   *(str(ROOT / source) for source in sources), "-o", str(executable)]
        report["compiler_command"] = command
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / "build.log").write_text(compiled.stdout + compiled.stderr)
        compiled.check_returncode()
        run = [str(executable), str(program)]
        if rows:
            replay = output / "direction-records.bin"
            with replay.open("xb") as stream:
                stream.write(b"RLYDBL1\0" + struct.pack("<I", len(rows)))
                for row in rows:
                    stream.write(struct.pack("<8I", *row))
            report["replay_sha256"] = digest(replay)
            run.append(str(replay))
        start = time.monotonic()
        tested = subprocess.run(run, capture_output=True, text=True, timeout=60)
        (output / "oracle.log").write_text(tested.stdout + tested.stderr)
        report.update(exit_code=tested.returncode, result=tested.stdout.strip(),
                      elapsed_seconds=round(time.monotonic() - start, 3), executable_sha256=digest(executable))
        tested.check_returncode()
        if {name: digest(ROOT / name) for name in dependencies} != hashes or digest(program) != PROGRAM_SHA256:
            raise RuntimeError("Oracle source/program inputs changed during validation")
        if live and {name: digest(name) for name in live["sha256"]} != live["sha256"]:
            raise RuntimeError("Driving capture changed during validation")
        report["status"] = "pass"
        print(tested.stdout, end="")
    finally:
        with (output / "result.json").open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()
    check(args.program, args.output, args.capture)


if __name__ == "__main__":
    main()
