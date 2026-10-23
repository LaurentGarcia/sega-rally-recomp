#!/usr/bin/env python3
"""Compare bounded native angle folding against the owner's verified program."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PROGRAM_SHA256 = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
LEAF_SHA256 = "bd67bceb1abed34efad3c73b3b3ac663d5bc5119dee5e58ab870c7a68cc80ed2"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--exhaustive-folded", action="store_true")
    args = parser.parse_args()
    program = args.program.resolve()
    image = program.read_bytes()
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        parser.error("program is not the verified Revision B image")
    if hashlib.sha256(image[0x27b28:0x27bc4]).hexdigest() != LEAF_SHA256:
        parser.error("angle function identity mismatch")
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "build") or output == ROOT / "build":
        parser.error("private evidence must be beneath this checkout's build/")
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = ["platform/ps2/tests/angle_fold_oracle.cpp", "platform/ps2/native/angle_fold.cpp",
               "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
    executable = output / "angle-fold-oracle"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
               "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
               *(str(ROOT / source) for source in sources), "-o", str(executable)]
    built = subprocess.run(command, capture_output=True, text=True)
    (output / "build.log").write_text(built.stdout + built.stderr)
    built.check_returncode()
    run = [str(executable), str(program)]
    if args.exhaustive_folded:
        run.append("--exhaustive-folded")
    start = time.monotonic()
    tested = subprocess.run(run, capture_output=True, text=True)
    elapsed = time.monotonic() - start
    (output / "oracle.log").write_text(tested.stdout + tested.stderr)
    dependencies = sources + ["platform/ps2/native/angle_fold.h", "src/runtime/cpu.h",
                              "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
                              "platform/ps2/tools/check_angle_fold.py"]
    report = {
        "schema": 1, "scope": "Single turn correction, nearest/even, finite binary32 magnitude <= round32(2*pi)",
        "program_sha256": PROGRAM_SHA256, "leaf_sha256": LEAF_SHA256,
        "compiler_command": command, "exit_code": tested.returncode, "elapsed_seconds": round(elapsed, 3),
        "result": tested.stdout.strip(), "oracle_executable_sha256": digest(executable),
        "sources_sha256": {source: digest(ROOT / source) for source in dependencies},
        "exhaustive_folded_interval": args.exhaustive_folded,
        "all_in_domain_bits_enumerated": False, "exception_status_verified": False,
        "live_caller_range_verified": False, "race_integration_verified": False, "physical_ps2_verified": False,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    tested.check_returncode()


if __name__ == "__main__":
    main()
