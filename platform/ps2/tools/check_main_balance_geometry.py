#!/usr/bin/env python3
"""Compare the bounded native balance geometry tail with the original region."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block
from main_motion_capture import SOURCES as MOTION_SOURCES, read_motion_capture, motion_envelopes

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
REGION = (0x382f8, 0x38658, "f06980f8870d7e5471caa73f1d7f5be9a305ec45d8876bb8fd3f871fddcb246c")
SOURCES = list(dict.fromkeys([
    "platform/ps2/native/main_balance_geometry.h", "platform/ps2/native/main_balance_geometry.cpp",
    "platform/ps2/native/rounded_affine_transform.h", "platform/ps2/native/rounded_affine_transform.cpp",
    "platform/ps2/tests/main_balance_geometry_cases.h", "platform/ps2/tests/main_balance_geometry_oracle.cpp",
    "platform/ps2/tools/check_main_balance_geometry.py", *MOTION_SOURCES,
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp",
    "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]))


def geometry_rows(events, vehicle_events, marker):
    rows, provenance = [], []
    for group in motion_envelopes(events,vehicle_events,marker):
        pre,post=group[2:4]
        regs=pre["registers"]
        if pre["object_words"]!=post["object_words"] or pre["registers"][30] or post["registers"][30]:
            raise ValueError("Balance geometry changed object storage or zero scratch")
        vector,output=regs[3],regs[15]
        if post["registers"][3]!=vector or post["registers"][15]!=output or post["registers"][9]!=pre["object"] or \
           post["registers"][10]!=regs[10]:
            raise ValueError("Balance geometry changed preserved pointers/index")
        inputs=[regs[6],regs[8],regs[5],regs[4],regs[14],regs[13],
                pre["object_words"][29],pre["object_words"][15],regs[10],
                *block(pre,"balance_vector",vector,3),*block(pre,"balance_output",output,3),
                *block(pre,"motion_prior",0x214264,1)]
        result=[*block(post,"balance_vector",vector,3),*block(post,"balance_output",output,3),
                *block(post,"motion_prior",0x214264,1)]
        if result[-1]!=regs[10]:
            raise ValueError("Balance geometry published index differs")
        rows.append(tuple(inputs+result))
        provenance.append({"call_id":pre["call_id"],"frame":pre["frame"],"object":pre["object"],
            "instructions":post["instructions"]-pre["instructions"],"selected_index":regs[10],
            "negative_first_longitudinal":bool(regs[6]&0x80000000 and regs[6]&0x7fffffff)})
    return rows,provenance


def read_capture(directory):
    events,vehicle_events,marker,hashes=read_motion_capture(directory)
    rows,provenance=geometry_rows(events,vehicle_events,marker)
    return rows,provenance,hashes


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
            raise ValueError("balance geometry tail instruction range differs")
        output=private_output(args.output)
        rows,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],{})
    except (ValueError,OSError) as error: parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","balance-geometry-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(args.tgp_program.resolve()),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),
            *(str(ROOT/name) for name in ("platform/ps2/native/main_balance_geometry.cpp",
                "platform/ps2/native/rounded_affine_transform.cpp","platform/ps2/tests/main_balance_geometry_oracle.cpp",
                "src/runtime/cpu.cpp","src/refcore/i960_ref.cpp","src/runtime/tgp.cpp")),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,
        "region":REGION,"sources_sha256":initial,"commands":commands,"capture_sha256":capture_hashes,
        "live_records":len(rows),"live_unique_inputs":len({row[:16] for row in rows}),"live_pairs":provenance,
        "live_negative_first_longitudinal":sum(p["negative_first_longitudinal"] for p in provenance),"live_comparison_verified":False,
        "physical_console_verified":False,"native_gameplay_connected":False,
        "scope":"Complete bounded balance geometry/publication tail; earlier helper dependencies and driving loop remain unconnected"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(args.program.resolve()),str(args.tgp_program.resolve())]
        if rows:
            records=output/"private-geometry.bin"
            records.write_bytes(b"MBGEOM01"+struct.pack("<I",len(rows))+b"".join(struct.pack("<23I",*row) for row in rows))
            report["records_sha256"]=digest(records);run.append(str(records))
        result=subprocess.run(run,capture_output=True,text=True,timeout=90)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),
                      generated_sha256=digest(generated),generator_sha256=digest(generator))
        result.check_returncode()
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith("MAIN_BALANCE_GEOMETRY_ORACLE PASS ") or \
           f" live={len(rows)} " not in result.stdout:
            raise RuntimeError("Balance geometry comparison lacks exact live completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or \
           any(digest(path)!=expected for path,_,expected in images):
            raise RuntimeError("Balance geometry source/private input changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows)
        print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__": main()
