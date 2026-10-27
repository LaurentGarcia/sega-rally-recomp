#!/usr/bin/env python3
"""Check the complete native balance operation against original helper calls."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from full_step_capture import SOURCES as CAPTURE_SOURCES, read_full_step_capture, blocks

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
NATIVE = ["main_balance_update", "main_balance_math", "main_balance_geometry", "vehicle_arithmetic",
          "vehicle_parameter_selection", "paired_balance", "cabinet_steering", "rounded_affine_transform"]
SOURCES = list(dict.fromkeys([*(f"platform/ps2/native/{name}.{extension}" for name in NATIVE for extension in ("h", "cpp")),
    "platform/ps2/tests/main_balance_update_adapter.h", "platform/ps2/tests/main_balance_update_oracle.cpp",
    "platform/ps2/tools/check_main_balance_update.py", *CAPTURE_SOURCES,
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/cpu.h", "src/runtime/cpu.cpp",
    "src/runtime/fast_paths.h", "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]))


def balance_records(groups):
    result = bytearray(b"MBUPDT01" + struct.pack("<I", len(groups)))
    for group in groups:
        pre, post = group[1:3]
        before, after = blocks(pre), blocks(post)
        if before.keys() != after.keys():
            raise ValueError("Balance observation block identities changed")
        result += struct.pack("<34I4Q", pre["frame"], pre["ac"], *pre["registers"], *pre["fp_bits"])
        result += struct.pack("<I", len(before))
        for key, values in before.items():
            expected = after[key]
            if len(values) != len(expected):
                raise ValueError("Balance observation block length changed")
            result += struct.pack("<2I", key[1], len(values))
            result += struct.pack(f"<{len(values)*2}I", *values, *expected)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--tgp-program", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        for path, size, expected in ((args.program,2097152,PROGRAM_SHA),(args.tgp_program,6660,FIRMWARE_SHA)):
            if path.stat().st_size != size or digest(path) != expected:
                raise ValueError("Balance private image differs: " + str(path))
        groups, summary, paths, capture_hashes = read_full_step_capture(args.capture)
        records = balance_records(groups)
        output = private_output(args.output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    records_path = output/"private-balance.bin"
    records_path.write_bytes(records)
    generator, generated, executable = (output/name for name in ("m2tgprecomp","private-tgp.cpp","balance-update-oracle"))
    common = [compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/native")]
    commands = [common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(args.tgp_program.resolve()),str(generated)],
        common+[*(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE),
                str(ROOT/"platform/ps2/tests/main_balance_update_oracle.cpp"),str(ROOT/"src/runtime/cpu.cpp"),
                str(ROOT/"src/refcore/i960_ref.cpp"),str(ROOT/"src/runtime/tgp.cpp"),str(generated),"-o",str(executable)]]
    report = {"schema":1,"status":"failed","sources_sha256":initial,"capture_sha256":capture_hashes,
        "program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,"records_sha256":digest(records_path),
        "commands":commands,"live_records":len(groups),"first_frame":groups[0][0]["frame"],"last_frame":groups[-1][0]["frame"],
        "scope":"Complete bounded balance helper, with each original caller state supplied independently; ordered sound requests compared, native sound transport excluded",
        "successive_native_state_proven":False,"physical_console_verified":False,"native_gameplay_connected":False}
    try:
        for i, command in enumerate(commands):
            result = subprocess.run(command,capture_output=True,text=True,timeout=90)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr)
            result.check_returncode()
        result = subprocess.run([str(executable),str(args.program.resolve()),str(args.tgp_program.resolve()),str(records_path)],capture_output=True,text=True,timeout=120)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),generated_sha256=digest(generated))
        result.check_returncode()
        if len(result.stdout.splitlines()) != 1 or not result.stdout.startswith(f"MAIN_BALANCE_UPDATE_ORACLE PASS live={len(groups)} "):
            raise RuntimeError("Balance original comparison lacks exact completion marker")
        if initial != {name:digest(ROOT/name) for name in SOURCES} or capture_hashes != {name:digest(Path(name)) for name in capture_hashes} or \
           digest(args.program) != PROGRAM_SHA or digest(args.tgp_program) != FIRMWARE_SHA:
            raise RuntimeError("Balance source or capture changed during proof")
        report["status"] = "passed"
        print(result.stdout,end="")
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__ == "__main__":
    main()
