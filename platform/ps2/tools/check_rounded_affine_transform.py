#!/usr/bin/env python3
"""Compare the typed native affine transform with verified private firmware."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output

FIRMWARE_SIZE = 6660
FIRMWARE_SHA256 = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
SOURCES = [
    "platform/ps2/native/rounded_affine_transform.h",
    "platform/ps2/native/rounded_affine_transform.cpp",
    "platform/ps2/tests/rounded_affine_transform_cases.h",
    "platform/ps2/tests/rounded_affine_transform_oracle.cpp",
    "platform/ps2/tools/check_rounded_affine_transform.py",
    "platform/ps2/tools/check_contact_neighborhood.py",
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h",
    "src/runtime/tgp.cpp", "src/runtime/fast_paths.h",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tgp-program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    firmware = args.tgp_program.resolve()
    if firmware.stat().st_size != FIRMWARE_SIZE or digest(firmware) != FIRMWARE_SHA256:
        parser.error("firmware does not match the verified private image")
    try:
        output = private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT / name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    generator, generated, executable = (output / x for x in ("m2tgprecomp", "private-tgp.cpp", "rounded-affine-transform-oracle"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src")]
    commands = [
        common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
        [str(generator), str(firmware), str(generated)],
        common + ["-DM2TGP_WITH_HOOK", "-I", str(ROOT / "platform/ps2/native"),
                  str(ROOT / "platform/ps2/tests/rounded_affine_transform_oracle.cpp"),
                  str(ROOT / "platform/ps2/native/rounded_affine_transform.cpp"),
                  str(ROOT / "src/runtime/tgp.cpp"), str(generated), "-o", str(executable)],
    ]
    report = {"schema": 1, "status": "failed", "firmware_sha256": FIRMWARE_SHA256,
              "sources_sha256": initial, "commands": commands,
              "scope": "Bounded point transform, translation-first separately rounded binary32 arithmetic",
              "matrix_stack_verified": False, "live_transform_verified": False,
              "physical_console_verified": False}
    try:
        for i, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (output / f"build-{i}.log").write_text(result.stdout + result.stderr)
            result.check_returncode()
        result = subprocess.run([str(executable), str(firmware)], capture_output=True, text=True, timeout=60)
        (output / "oracle.log").write_text(result.stdout + result.stderr)
        report.update(result=result.stdout.strip(), exit_code=result.returncode,
                      executable_sha256=digest(executable), generated_sha256=digest(generated),
                      generator_sha256=digest(generator))
        result.check_returncode()
        if "ROUNDED_AFFINE_ORACLE PASS" not in result.stdout:
            raise RuntimeError("oracle did not emit its completion marker")
        if {name: digest(ROOT / name) for name in SOURCES} != initial or digest(firmware) != FIRMWARE_SHA256:
            raise RuntimeError("source or private firmware changed during comparison")
        report["status"] = "passed"
        print(result.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
