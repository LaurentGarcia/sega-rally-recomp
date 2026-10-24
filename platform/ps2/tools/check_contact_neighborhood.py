#!/usr/bin/env python3
"""Privately compare bounded contact indexing with the owner's Revision B ROM."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PROGRAM_SHA256 = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
QUERY_SHA256 = "ff2df39653904b67d7172b6765604008e5c7c981bd003836e980943bf821a736"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def private_output(path):
    root = ROOT / "build"
    resolved = path.resolve()
    if root.is_symlink() or resolved == root or not resolved.is_relative_to(root):
        raise ValueError("private evidence must be beneath this checkout's real build directory")
    return resolved


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    image = program.read_bytes()
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        parser.error("program is not the verified Revision B image")
    if hashlib.sha256(image[0x2abc0:0x2acbc]).hexdigest() != QUERY_SHA256:
        parser.error("contact-query preparation identity mismatch")
    try:
        output = private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = ["platform/ps2/tests/contact_neighborhood_oracle.cpp",
               "platform/ps2/native/contact_neighborhood.cpp", "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    dependencies = sources + ["platform/ps2/native/contact_neighborhood.h", "src/runtime/cpu.h",
                              "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
                              "platform/ps2/tools/check_contact_neighborhood.py"]
    initial = {source: digest(ROOT / source) for source in dependencies}
    executable = output / "contact-neighborhood-oracle"
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-fno-fast-math", "-ffp-contract=off",
               "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
               *(str(ROOT / source) for source in sources), "-o", str(executable)]
    built = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(built.stdout + built.stderr)
    built.check_returncode()
    started = time.monotonic()
    tested = subprocess.run([str(executable), str(program)], capture_output=True, text=True, timeout=60)
    elapsed = time.monotonic() - started
    (output / "oracle.log").write_text(tested.stdout + tested.stderr)
    if {source: digest(ROOT / source) for source in dependencies} != initial:
        raise RuntimeError("oracle source changed during validation")
    if digest(program) != PROGRAM_SHA256:
        raise RuntimeError("program changed during validation")
    report = {
        "schema": 1, "scope": "Signed segment rejection and ordered cyclic contact neighborhood only",
        "program_sha256": PROGRAM_SHA256, "query_sha256": QUERY_SHA256,
        "compiler_command": command, "sources_sha256": initial,
        "oracle_executable_sha256": digest(executable), "exit_code": tested.returncode,
        "elapsed_seconds": round(elapsed, 3), "result": tested.stdout.strip(),
        "signed16_inputs_exhaustive_per_count": True,
        "counts": [0, 1, 2, 10, 300, 32767, 32768, 65535],
        "count_response": "Explicit fixture input; TGP count relationship requires separate probe",
        "all_supported_counts_exhaustive": False, "live_caller_range_verified": False,
        "intersection_math_verified": False, "race_integration_verified": False,
        "physical_ps2_verified": False,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    if tested.returncode:
        print(tested.stderr, end="")
    tested.check_returncode()


if __name__ == "__main__":
    main()
