#!/usr/bin/env python3
"""Compare all cabinet input bytes with the owner's verified Revision B program."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PROGRAM_SHA256 = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
STEERING_SHA256 = "1363e4086b6a24a4c452c6c7b7912e51e6bf12c6ac90b6117b3ac0278b22de88"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def private_output(path):
    build = ROOT / "build"
    output = Path(path).resolve()
    if build.is_symlink() or output == build or not output.is_relative_to(build):
        raise ValueError("private evidence must stay below this checkout's real build directory")
    return output


def unsigned(value, width, name):
    if type(value) is not int or not 0 <= value < 1 << width:
        raise ValueError("invalid steering capture " + name)
    return value


def steering_pairs(records):
    pending, finished, rows, provenance = {}, set(), [], []
    for record in records:
        if not isinstance(record, dict) or record.get("schema") != 1:
            raise ValueError("invalid steering record schema")
        event = record.get("event")
        expected_pc = {"steering_pre": 0x5d85d8, "steering_post": 0x5d8664}.get(event)
        if expected_pc is None or record.get("pc") != expected_pc:
            raise ValueError("wrong steering instruction boundary")
        pair = unsigned(record.get("pair_id"), 32, "pair_id")
        for field in ("frame", "ac", "return_pc", "frame_pointer"):
            unsigned(record.get(field), 32, field)
        for field in ("instructions", "cycles", "board_ns"):
            unsigned(record.get(field), 64, field)
        if record["ac"] >> 30 or record.get("input_address") != 0x202050:
            raise ValueError("unsupported steering input or rounding control")
        sample = unsigned(record.get("input_byte"), 8, "sample")
        registers = record.get("registers")
        if not isinstance(registers, list) or len(registers) != 32:
            raise ValueError("steering record requires all registers")
        for value in registers:
            unsigned(value, 32, "register")
        if record["frame_pointer"] != registers[31]:
            raise ValueError("steering frame pointer differs from registers")
        if event == "steering_pre":
            if pair in pending or pair in finished or len(pending) + len(finished) >= 16384:
                raise ValueError("duplicate or excessive steering pair")
            if record["return_pc"] != registers[30]:
                raise ValueError("steering entry return register differs")
            pending[pair] = record
            continue
        before = pending.pop(pair, None)
        if before is None:
            raise ValueError("unpaired or duplicate steering output")
        for field in ("input_byte", "return_pc", "frame_pointer", "frame"):
            if record[field] != before[field]:
                raise ValueError("steering pair identity changed")
        steps = record["instructions"] - before["instructions"]
        # This counter also includes interrupt handlers. Isolated original
        # execution is exactly27..30 instructions; a live sample can span more.
        # Preserve a bounded window and report it separately from oracle cost.
        if not 27 <= steps <= 4096 or any(record[k] < before[k] for k in ("cycles", "board_ns")):
            raise ValueError("steering pair order or instruction span differs")
        if registers[17] != before["registers"][30] or registers[30] != 0:
            raise ValueError("steering return registers differ")
        rows.append((sample, registers[16]))
        provenance.append({"pair_id": pair, "frame": record["frame"], "instructions": steps,
                           "return_pc": record["return_pc"], "ac": record["ac"],
                           "exceeds_isolated_instruction_span": steps > 30})
        finished.add(pair)
    if pending or not rows:
        raise ValueError("absent or incomplete steering pairs")
    return rows, provenance


def capture_inputs(capture):
    capture = private_output(capture)
    marker, stream, validation = capture / "capture.json", capture / "steering.jsonl", capture.parent / "validation.json"
    for path, limit in ((marker, 16384), (stream, 16 * 1024 * 1024), (validation, 1024 * 1024)):
        if path.stat().st_size > limit:
            raise ValueError("steering evidence exceeds reader bound")
    hashes = {str(path): digest(path) for path in (marker, stream, validation)}
    metadata, checked = json.loads(marker.read_text()), json.loads(validation.read_text())
    if metadata.get("schema") != 1 or metadata.get("complete") is not True or \
       checked.get("status") != "passed" or checked.get("exact_checkpoint_match") is not True:
        raise ValueError("steering capture needs completed observer parity validation")
    for path in (marker, stream):
        if checked.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("steering capture differs from validated identity")
    records = [json.loads(line) for line in stream.read_text().splitlines()]
    rows, provenance = steering_pairs(records)
    if metadata.get("steering_entries") != metadata.get("steering_completions") or \
       not metadata.get("steering_entries") or metadata.get("steering_samples") != len(rows):
        raise ValueError("steering completion counts differ")
    return rows, {"records": len(rows), "samples": sorted({row[0] for row in rows}),
                  "pairs": provenance, "sha256": hashes,
                  "reference": "Translated host reference; not independent PCB validation"}


def check(program, output, capture=None):
    program = Path(program).resolve()
    image = program.read_bytes()
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        raise ValueError("program is not the verified Revision B image")
    if hashlib.sha256(image[0x395d8:0x39668]).hexdigest() != STEERING_SHA256:
        raise ValueError("steering conversion identity mismatch")
    output = private_output(output)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        raise ValueError("C++20 compiler required")
    rows, captured = capture_inputs(capture) if capture is not None else ([], None)
    output.mkdir(parents=True, exist_ok=False)
    sources = ["platform/ps2/tests/cabinet_steering_oracle.cpp",
               "platform/ps2/native/cabinet_steering.cpp", "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    dependencies = sources + ["platform/ps2/native/cabinet_steering.h", "src/runtime/cpu.h",
                              "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
                              "platform/ps2/tools/check_cabinet_steering.py"]
    hashes = {source: digest(ROOT / source) for source in dependencies}
    executable = output / "cabinet-steering-oracle"
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-fno-fast-math", "-ffp-contract=off",
               "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
               *(str(ROOT / source) for source in sources), "-o", str(executable)]
    built = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(built.stdout + built.stderr)
    built.check_returncode()
    started = time.monotonic()
    run_command = [str(executable), str(program)]
    if captured:
        inputs = output / "captured-inputs.txt"
        inputs.write_text("".join(f"{sample} {result}\n" for sample, result in rows))
        run_command.append(str(inputs))
    tested = subprocess.run(run_command, capture_output=True, text=True, timeout=30)
    elapsed = time.monotonic() - started
    (output / "oracle.log").write_text(tested.stdout + tested.stderr)
    if {source: digest(ROOT / source) for source in dependencies} != hashes or digest(program) != PROGRAM_SHA256:
        raise RuntimeError("steering oracle inputs changed during validation")
    if captured and any(digest(path) != sha for path, sha in captured["sha256"].items()):
        raise RuntimeError("steering capture changed during validation")
    report = {
        "schema": 1, "scope": "Exhaustive stateless cabinet byte conversion only",
        "status": "passed" if tested.returncode == 0 else "failed",
        "program_sha256": PROGRAM_SHA256, "steering_sha256": STEERING_SHA256,
        "source_sha256": hashes, "compiler_command": command,
        "executable_sha256": digest(executable), "exit_code": tested.returncode,
        "elapsed_seconds": round(elapsed, 3), "result": tested.stdout.strip(),
        "all_256_samples_checked": tested.returncode == 0, "stateful_filter_verified": False,
        "live_capture": captured, "native_gameplay_connected": False,
        "physical_ps2_verified": False,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    if tested.returncode:
        print(tested.stderr, end="")
    tested.check_returncode()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()
    try:
        check(args.program, args.output, args.capture)
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
