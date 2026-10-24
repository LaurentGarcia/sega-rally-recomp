#!/usr/bin/env python3
"""Privately confirm course counts using the owner's uploaded TGP firmware."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_contact_neighborhood import PROGRAM_SHA256, ROOT, digest, private_output

IDENTITIES = {
    "program": (2097152, PROGRAM_SHA256),
    "tgp_program": (6660, "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"),
    "tgp_tables": (262144, "3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04"),
    "copro_data": (8388608, "22d65f223f3b341088c013666c8f35b522ef9b4c0a383b36f7c0b683396c524f"),
    "main_data": (37748736, "2c55fca2ed44c87a8886cfcfb0638ad8e25f5fac0e9fbe5d4cf990ba950cc8e1"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in IDENTITIES:
        parser.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve() for name in IDENTITIES}
    for name, path in inputs.items():
        size, expected = IDENTITIES[name]
        if path.stat().st_size != size or digest(path) != expected:
            parser.error(f"{name} does not match the verified private image")
    try:
        output = private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    dependencies = ["tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp",
                    "src/runtime/fast_paths.h", "platform/ps2/tests/contact_count_probe.cpp",
                    "platform/ps2/tools/check_contact_count.py",
                    "platform/ps2/tools/check_contact_neighborhood.py"]
    initial = {source: digest(ROOT / source) for source in dependencies}
    generator = output / "m2tgprecomp"
    generated = output / "private-tgp.cpp"
    executable = output / "contact-count-probe"
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
              "-I", str(ROOT / "src")]
    commands = [
        common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
        [str(generator), str(inputs["tgp_program"]), str(generated)],
        common + [str(ROOT / "platform/ps2/tests/contact_count_probe.cpp"),
                  str(ROOT / "src/runtime/tgp.cpp"), str(generated), "-o", str(executable)],
    ]
    for index, command in enumerate(commands):
        built = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / f"build-{index}.log").write_text(built.stdout + built.stderr)
        built.check_returncode()
    tested = subprocess.run([str(executable), *(str(path) for path in inputs.values())],
                            capture_output=True, text=True, timeout=15)
    (output / "probe.log").write_text(tested.stdout + tested.stderr)
    if {source: digest(ROOT / source) for source in dependencies} != initial:
        raise RuntimeError("count-probe source changed during validation")
    if any(digest(path) != IDENTITIES[name][1] for name, path in inputs.items()):
        raise RuntimeError("private input changed during validation")
    report = {
        "schema": 1, "scope": "Command7 reads one copro-data word at descriptor.word0; matches CPU-side header count",
        "input_sha256": {name: IDENTITIES[name][1] for name in inputs},
        "sources_sha256": initial, "commands": commands,
        "generator_sha256": digest(generator), "private_generated_sha256": digest(generated),
        "probe_executable_sha256": digest(executable), "result": tested.stdout.strip(),
        "exit_code": tested.returncode, "descriptors_verified": 8 if tested.returncode == 0 else 0,
        "firmware_instruction_bound_per_command": 10000,
        "intersection_math_verified": False, "live_caller_range_verified": False,
        "physical_ps2_verified": False,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(tested.stdout, end="")
    if tested.returncode:
        print(tested.stderr, end="")
    tested.check_returncode()


if __name__ == "__main__":
    main()
