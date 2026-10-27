#!/usr/bin/env python3
"""Compare native post-balance motion with complete observed stage boundaries."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from extract_main_motion import SOURCES as READER_SOURCES

NATIVE = ["main_motion_update", "driving_assets", "vehicle_arithmetic", "angle_fold",
          "recovered_scalar_metric", "rounded_cross_product", "scene_contact_damping"]
HEADERS = ["main_candidate", "main_balance_update", "main_balance_geometry", "vehicle_parameter_selection",
           "rounded_affine_transform", "paired_balance", "contact_result", "contact_search", "contact_projection",
           "segment_selection"]
SOURCES = list(dict.fromkeys([*(f"platform/ps2/native/{name}.{ext}" for name in NATIVE for ext in ("h","cpp")),
    *(f"platform/ps2/native/{name}.h" for name in HEADERS),
    "platform/ps2/tests/main_motion_update_adapter.h", "platform/ps2/tests/main_motion_update_live.cpp",
    "platform/ps2/tools/check_main_motion_update.py", *READER_SOURCES]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--records", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    try:
        output = private_output(args.output)
        if args.records.stat().st_size > 16*1024*1024:
            raise ValueError("Motion record size exceeds bounded policy")
        initial = {name:digest(ROOT/name) for name in SOURCES}
        private_hashes = {str(path.resolve()):digest(path) for path in (args.records,args.assets)}
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output.mkdir(parents=True, exist_ok=False)
    executable = output/"motion-live"
    command = [compiler,"-std=c++20","-O1" if args.sanitize else "-O2","-Wall","-Wextra","-Werror",
        "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native")]
    if args.sanitize:
        command += ["-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    command += [*(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE),
        str(ROOT/"platform/ps2/tests/main_motion_update_live.cpp"),"-o",str(executable)]
    report = {"schema":1,"status":"failed","sources_sha256":initial,"private_inputs_sha256":private_hashes,
        "sanitizers":args.sanitize,"build_command":command,
        "scope":"Observed post-balance boundaries, independent stage inputs; original memory and ordered sound bytes",
        "successive_native_state_proven":False,"native_gameplay_connected":False,"physical_console_verified":False}
    try:
        result = subprocess.run(command,capture_output=True,text=True,timeout=90)
        (output/"build.log").write_text(result.stdout+result.stderr)
        result.check_returncode()
        result = subprocess.run([str(executable),str(args.assets.resolve()),str(args.records.resolve())],
                                capture_output=True,text=True,timeout=60)
        (output/"check.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable))
        result.check_returncode()
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith("MAIN_MOTION_UPDATE_LIVE PASS steps="):
            raise ValueError("Motion proof lacks exact completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or private_hashes!={name:digest(Path(name)) for name in private_hashes}:
            raise ValueError("Motion proof sources or private inputs changed")
        report["status"]="passed"
        print(result.stdout,end="")
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__":
    main()
