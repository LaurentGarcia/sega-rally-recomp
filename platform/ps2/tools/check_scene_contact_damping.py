#!/usr/bin/env python3
"""Compare native scene-relative damping with the original private programs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
REGIONS = ((0x36ee0, 0x37024, "21a7eed194b587389e78b2a960ddc877ece2fd4e50cdb0d4e7fd62819d24081e"),
           (0x530 * 4, 0x53a * 4, "5a95f531c51bca486f6af7274d8762c7bfe2c74881df259d9c218297d86e5178"))
NATIVE = ("scene_contact_damping", "vehicle_arithmetic", "recovered_scalar_metric")
SOURCES = [*(f"platform/ps2/native/{name}.{suffix}" for name in NATIVE for suffix in ("h", "cpp")),
    "platform/ps2/native/rounded_affine_transform.h", "platform/ps2/tests/scene_contact_damping_oracle.cpp",
    "platform/ps2/tools/check_scene_contact_damping.py", "platform/ps2/tools/check_contact_neighborhood.py",
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp",
    "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--tgp-program", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    images = ((args.program, 2097152, PROGRAM_SHA), (args.tgp_program, 6660, FIRMWARE_SHA))
    try:
        for (path, size, expected), (start, end, region_hash) in zip(images, REGIONS):
            if path.stat().st_size != size or digest(path) != expected:
                raise ValueError("Private input differs: " + str(path))
            if hashlib.sha256(path.read_bytes()[start:end]).hexdigest() != region_hash:
                raise ValueError("Original instruction region differs")
        output = private_output(args.output)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT / name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    generator, generated, executable = (output / name for name in ("m2tgprecomp", "private-tgp.cpp", "scene-oracle"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src")]
    commands = [common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
        [str(generator), str(args.tgp_program.resolve()), str(generated)],
        common + ["-I", str(ROOT / "platform/ps2/native"),
            *(str(ROOT / f"platform/ps2/native/{name}.cpp") for name in NATIVE),
            *(str(ROOT / name) for name in ("platform/ps2/tests/scene_contact_damping_oracle.cpp",
                "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp", "src/runtime/tgp.cpp")),
            str(generated), "-o", str(executable)]]
    report = {"schema": 1, "status": "failed", "program_sha256": PROGRAM_SHA,
        "firmware_sha256": FIRMWARE_SHA, "regions": REGIONS, "sources_sha256": initial,
        "commands": commands, "physical_console_verified": False, "live_active_scene_cases": 0,
        "scope": "Bounded scene-relative damping only; synthetic original CPU and firmware comparison"}
    try:
        for index, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, text=True, timeout=90)
            (output / f"build-{index}.log").write_text(result.stdout + result.stderr)
            result.check_returncode()
        result = subprocess.run([str(executable), str(args.program.resolve()), str(args.tgp_program.resolve())],
                                capture_output=True, text=True, timeout=90)
        (output / "oracle.log").write_text(result.stdout + result.stderr)
        report.update(result=result.stdout.strip(), exit_code=result.returncode,
                      executable_sha256=digest(executable), generated_sha256=digest(generated),
                      generator_sha256=digest(generator))
        result.check_returncode()
        match = re.fullmatch(r"SCENE_CONTACT_DAMPING_ORACLE PASS cases=(\d+) accepted=(\d+) corrected=(\d+) rejected=(\d+) cpu_instructions=(\d+) geometry_instructions=(\d+) hash=([0-9a-f]{16})\n", result.stdout)
        if not match or int(match[1]) != int(match[2]) + int(match[4]) or not 0 < int(match[3]) < int(match[2]):
            raise RuntimeError("Missing complete numerical comparison")
        if initial != {name: digest(ROOT / name) for name in SOURCES} or any(digest(path) != expected for path, _, expected in images):
            raise RuntimeError("Source/private input changed during comparison")
        report["status"] = "passed"
        print(result.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
