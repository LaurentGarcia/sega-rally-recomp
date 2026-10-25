#!/usr/bin/env python3
"""Privately compare bounded native normalization with original CPU instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from check_contact_neighborhood import ROOT, PROGRAM_SHA256, digest, private_output

START, END = 0x348ec, 0x34968
REGION_SHA256 = "50d22d6c9883588bd118027412792f2fd91ec535756aeac1d6b8fd411af9b847"
COMPILED = ["platform/ps2/tests/contact_normalization_oracle.cpp", "platform/ps2/native/contact_normalization.cpp",
            "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp"]
DEPENDENCIES = COMPILED + ["platform/ps2/native/contact_normalization.h", "platform/ps2/tests/contact_normalization_cases.h",
    "src/runtime/cpu.h", "src/runtime/fast_paths.h", "src/refcore/i960_ref.h",
    "platform/ps2/tools/check_contact_normalization.py", "platform/ps2/tools/check_contact_neighborhood.py"]


def verify_image(image):
    if len(image) != 2097152 or hashlib.sha256(image).hexdigest() != PROGRAM_SHA256:
        raise ValueError("program is not the verified Revision B image")
    if hashlib.sha256(image[START:END]).hexdigest() != REGION_SHA256:
        raise ValueError("normalization region differs from verified identity")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()
    program = args.program.resolve()
    rows, provenance, input_hashes = [], None, {}
    dependencies = list(DEPENDENCIES)
    try:
        verify_image(program.read_bytes())
        output = private_output(args.output)
        if args.capture:
            from contact_normalization_live import read_capture, SOURCES
            rows, provenance, input_hashes = read_capture(args.capture.resolve())
            dependencies += list(SOURCES)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT / name) for name in dependencies}
    output.mkdir(parents=True, exist_ok=False)
    executable = output / "normalization-oracle"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src"),
        "-I", str(ROOT / "platform/ps2/native"), *(str(ROOT / name) for name in COMPILED), "-o", str(executable)]
    run = [str(executable), str(program)]
    report = {"schema": 1, "status": "failed", "scope": "Bounded isolated contact normalization",
        "program_sha256": PROGRAM_SHA256, "region": [START, END], "region_sha256": REGION_SHA256,
        "sources_sha256": initial, "compiler_command": command, "capture": provenance,
        "capture_inputs_sha256": input_hashes, "full_input_domain_exhaustive": False,
        "live_rows": len(rows), "live_helper_verified": False, "race_integration_verified": False,
        "physical_ps2_verified": False}
    if rows:
        row_path = output / "live-rows.txt"
        row_path.write_text("".join(" ".join(f"{v:08x}" for v in row) + "\n" for row in rows))
        report["live_rows_sha256"] = digest(row_path)
        run.append(str(row_path))
    try:
        built = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / "build.log").write_text(built.stdout + built.stderr)
        built.check_returncode()
        tested = subprocess.run(run, capture_output=True, text=True, timeout=60)
        (output / "oracle.log").write_text(tested.stdout + tested.stderr)
        report.update(result=tested.stdout.strip(), exit_code=tested.returncode, executable_sha256=digest(executable))
        tested.check_returncode()
        if f"CONTACT_NORMALIZATION_ORACLE PASS cases={59912 + len(rows)} " not in tested.stdout or f" live={len(rows)} " not in tested.stdout:
            raise RuntimeError("original comparison completion marker changed")
        if initial != {name: digest(ROOT / name) for name in dependencies} or digest(program) != PROGRAM_SHA256:
            raise RuntimeError("source or original program changed during comparison")
        if any(digest(Path(name)) != value for name, value in input_hashes.items()):
            raise RuntimeError("capture inputs changed during comparison")
        report.update(status="passed", live_helper_verified=bool(rows))
        print(tested.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
