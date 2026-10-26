#!/usr/bin/env python3
"""Compare native main-vehicle candidate preparation with the original region."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block
from main_vehicle_capture import SOURCES as VEHICLE_SOURCES, read_vehicle_capture, vehicle_envelopes

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
REGION = (0x34ff0, 0x35218, "b49f8290c4ca32b010985755a2243c4bd4fc70da9e666cfb9228aa2bd9cbaba8")
SOURCES = list(dict.fromkeys([
    "platform/ps2/native/main_candidate.h", "platform/ps2/native/main_candidate.cpp",
    "platform/ps2/native/recovered_scalar_metric.h", "platform/ps2/native/recovered_scalar_metric.cpp",
    "platform/ps2/tests/main_candidate_cases.h", "platform/ps2/tests/main_candidate_oracle.cpp",
    "platform/ps2/tools/check_main_candidate.py", *VEHICLE_SOURCES,
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp",
    "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]))


def prefix_rows(events, marker):
    """Extract exact22-word before/after values from validated parent groups.

    The observed marker being15 does not identify the gate branch: the marker
    may already have that value at entry. No branch count is inferred here.
    """
    rows, provenance = [], []
    for group in vehicle_envelopes(events, marker):
        pre, post = group[1:3]
        before, after = pre["object_words"], post["object_words"]
        if any(before[i] != after[i] for i in range(64) if i not in (5,6,7,8,9,10,11,13)):
            raise ValueError("Candidate prefix changed unrelated object storage")
        frame = pre["vehicle_frame_pointer"]
        values = []
        for event in (pre,post):
            values += event["object_words"][5:20]
            values += block(event,"prefix_global",0x213974,1)
            values += block(event,"candidate_position",frame+0x130,3)
            values += block(event,"candidate_orientation",frame+0x140,3)
        if len(values)!=44: raise AssertionError("Candidate row width")
        rows.append(tuple(values))
        provenance.append({"call_id":pre["call_id"],"frame":pre["frame"],"object":pre["object"],
            "instructions":post["instructions"]-pre["instructions"],
            "input_marker":values[15],"output_marker":values[37]})
    return rows, provenance


def read_capture(directory):
    events, marker, hashes = read_vehicle_capture(directory)
    rows, provenance = prefix_rows(events,marker)
    return rows, provenance, hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program",type=Path,required=True)
    parser.add_argument("--tgp-program",type=Path,required=True)
    parser.add_argument("--capture",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    images=[(args.program,2097152,PROGRAM_SHA),(args.tgp_program,6660,FIRMWARE_SHA)]
    try:
        for path,size,expected in images:
            if path.stat().st_size!=size or digest(path)!=expected: raise ValueError("private input differs: "+str(path))
        start,end,expected=REGION
        if hashlib.sha256(args.program.read_bytes()[start:end]).hexdigest()!=expected:
            raise ValueError("candidate prefix instruction range differs")
        output=private_output(args.output)
        rows,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],{})
    except (ValueError,OSError) as error: parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","candidate-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(args.tgp_program.resolve()),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),
            *(str(ROOT/name) for name in ("platform/ps2/native/main_candidate.cpp",
                "platform/ps2/native/recovered_scalar_metric.cpp","platform/ps2/tests/main_candidate_oracle.cpp",
                "src/runtime/cpu.cpp","src/refcore/i960_ref.cpp","src/runtime/tgp.cpp")),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,
        "region":REGION,"sources_sha256":initial,"commands":commands,"capture_sha256":capture_hashes,
        "live_records":len(rows),"live_unique_inputs":len({row[:22] for row in rows}),"live_pairs":provenance,
        "live_output_marker_15":sum(row[37]==15 for row in rows),
        "marker_count_is_branch_count":False,"live_comparison_verified":False,
        "physical_console_verified":False,"native_gameplay_connected":False,
        "scope":"Bounded main-callback prefix only; complete callback and driving loop remain unconnected"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(args.program.resolve()),str(args.tgp_program.resolve())]
        if rows:
            records=output/"private-candidates.bin"
            records.write_bytes(b"MCANDID1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<44I",*row) for row in rows))
            report["records_sha256"]=digest(records);run.append(str(records))
        result=subprocess.run(run,capture_output=True,text=True,timeout=90)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),
                      generated_sha256=digest(generated),generator_sha256=digest(generator))
        result.check_returncode()
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith("MAIN_CANDIDATE_ORACLE PASS ") or \
           f" live={len(rows)} " not in result.stdout:
            raise RuntimeError("Candidate comparison lacks exact live completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or \
           any(digest(path)!=expected for path,_,expected in images):
            raise RuntimeError("Candidate source/private input changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows)
        print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__": main()
