#!/usr/bin/env python3
"""Compare native pre-balance front with sealed original stage boundaries."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from extract_main_stages import SOURCES as READER_SOURCES

NATIVE = ["main_front_update", "driving_assets", "vehicle_arithmetic", "main_balance_math", "recovered_atangent", "contact_point_preparation", "rounded_affine_transform", "main_contact_slot", "contact_search", "contact_neighborhood", "contact_projection", "contact_result", "recovered_scalar_metric", "rounded_cross_product"]
HEADERS = ["main_candidate", "main_balance_update", "main_balance_geometry", "vehicle_parameter_selection", "paired_balance", "segment_selection", "main_contact_step", "candidate_contact_pass", "contact_pass_response", "contact_response", "contact_aggregate", "contact_normalization"]
SOURCES = list(dict.fromkeys([*(f"platform/ps2/native/{name}.{ext}" for name in NATIVE for ext in ("h","cpp")),
    *(f"platform/ps2/native/{name}.h" for name in HEADERS),
    "platform/ps2/tests/main_front_update_adapter.h", "platform/ps2/tests/main_front_update_live.cpp",
    "platform/ps2/tools/check_main_front_update.py", *READER_SOURCES]))


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
            raise ValueError("Front record size exceeds bounded policy")
        initial = {name:digest(ROOT/name) for name in SOURCES}
        private_hashes = {str(path.resolve()):digest(path) for path in (args.records,args.assets)}
        extraction_path=args.records.parent/"result.json"
        extraction=json.loads(extraction_path.read_text())
        asset_path=args.assets.with_suffix(".json")
        asset=json.loads(asset_path.read_text())
        if extraction.get("status")!="passed" or extraction.get("front_sha256")!=digest(args.records):
            raise ValueError("Front records lack passed extraction binding")
        if asset.get("output",{}).get("sha256")!=digest(args.assets):
            raise ValueError("Native assets lack exporter binding")
        sealed=extraction.get("capture_sha256")
        if not sealed or any(digest(Path(path))!=expected for path,expected in sealed.items()):
            raise ValueError("Original capture seal differs")
        private_hashes.update({str(path.resolve()):digest(path) for path in (extraction_path,asset_path)})
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output.mkdir(parents=True, exist_ok=False)
    executable = output/"front-live"
    command = [compiler,"-std=c++20","-O1" if args.sanitize else "-O2","-Wall","-Wextra","-Werror",
        "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native")]
    if args.sanitize:
        command += ["-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    command += [*(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE),
        str(ROOT/"platform/ps2/tests/main_front_update_live.cpp"),"-o",str(executable)]
    report = {"schema":1,"status":"failed","sources_sha256":initial,"private_inputs_sha256":private_hashes,
        "sealed_capture_sha256":sealed,"expected_steps":extraction["record_count"],
        "sanitizers":args.sanitize,"build_command":command,
        "scope":"Observed complete pre-balance stages, independently supplied initial state; original persistent and derived outputs",
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
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith(f"MAIN_FRONT_UPDATE_LIVE PASS steps={extraction['record_count']} "):
            raise ValueError("Front proof lacks exact completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or private_hashes!={name:digest(Path(name)) for name in private_hashes} or any(digest(Path(path))!=expected for path,expected in sealed.items()):
            raise ValueError("Front proof sources or private inputs changed")
        report["status"]="passed"
        print(result.stdout,end="")
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__":
    main()
