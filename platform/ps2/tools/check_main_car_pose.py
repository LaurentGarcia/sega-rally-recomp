#!/usr/bin/env python3
"""Privately compare typed main-car publication against verified original code."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from check_contact_neighborhood import ROOT, PROGRAM_SHA256, digest, private_output

START, END = 0x34078, 0x34130
LEAF_SHA256 = "16a9bdcd4eddd6a783f2fc0e08d9162ea7a0fe124fe602f348456f46f89ac7a0"
COMPILED = ["platform/ps2/tests/main_car_pose_oracle.cpp", "platform/ps2/native/main_car_pose.cpp",
            "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
DEPENDENCIES = COMPILED + ["platform/ps2/tests/main_car_pose_cases.h",
    "platform/ps2/native/main_car_pose.h", "platform/ps2/native/car_pose.h",
    "src/runtime/cpu.h", "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
    "platform/ps2/tools/check_main_car_pose.py", "platform/ps2/tools/check_contact_neighborhood.py"]


def verify_image(image):
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        raise ValueError("program is not the verified Revision B image")
    if hashlib.sha256(image[START:END]).hexdigest() != LEAF_SHA256:
        raise ValueError("main-car publication region differs from verified identity")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    try:
        verify_image(program.read_bytes())
        output = private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT / name) for name in DEPENDENCIES}
    output.mkdir(parents=True, exist_ok=False)
    executable = output / "main-car-pose-oracle"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-I", str(ROOT / "src"), "-I", str(ROOT / "platform/ps2/native"),
        *(str(ROOT / name) for name in COMPILED), "-o", str(executable)]
    report = {"schema": 1, "status": "failed", "scope": "Isolated raw-value main-car pose publication",
        "program_sha256": PROGRAM_SHA256, "region": [START, END], "leaf_sha256": LEAF_SHA256,
        "sources_sha256": initial, "compiler_command": command,
        "source_low_halfword_exhaustive": True, "full_input_domain_exhaustive": False,
        "live_helper_verified": False, "race_integration_verified": False, "physical_ps2_verified": False}
    try:
        built = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / "build.log").write_text(built.stdout + built.stderr)
        built.check_returncode()
        tested = subprocess.run([str(executable), str(program)], capture_output=True, text=True, timeout=60)
        (output / "oracle.log").write_text(tested.stdout + tested.stderr)
        report.update(result=tested.stdout.strip(), exit_code=tested.returncode, executable_sha256=digest(executable))
        tested.check_returncode()
        if "MAIN_CAR_POSE_ORACLE PASS cases=77208" not in tested.stdout:
            raise RuntimeError("original comparison did not emit its expected completion marker")
        if initial != {name: digest(ROOT / name) for name in DEPENDENCIES} or digest(program) != PROGRAM_SHA256:
            raise RuntimeError("source or original program changed during comparison")
        report["status"] = "passed"
        print(tested.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
