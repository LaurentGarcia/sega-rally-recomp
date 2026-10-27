#!/usr/bin/env python3
"""Compare native segment selection with the complete original CPU/geometry helpers."""
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

PROGRAM_SHA="bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA="68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
DATA_SHA="2c55fca2ed44c87a8886cfcfb0638ad8e25f5fac0e9fbe5d4cf990ba950cc8e1"
REGIONS=[(0x2aed0,0x2b0b0,"b8a4804004110f1395e93943864db50eb137b3466a164a2fb11ed5898f7235f6"),
         (0x2b0b0,0x2b174,"8dd76c9554c0ce6cb4610087b7da6aaff5c1769c836d034e7db273d3553e2f79")]
EDGE_REGION=(0x577,0x58a,"0482633af4096122bb8db69e0c5f6be4cf5d8b36da74a5e7da6084e14ad2da04")
SOURCES=list(dict.fromkeys([
    "platform/ps2/native/segment_selection.h","platform/ps2/native/segment_selection.cpp",
    "platform/ps2/native/contact_projection.h","platform/ps2/native/contact_projection.cpp",
    "platform/ps2/tests/segment_selection_cases.h","platform/ps2/tests/segment_selection_oracle.cpp",
    "platform/ps2/tests/contact_projection_cases.h","platform/ps2/tools/check_segment_selection.py",
    *MOTION_SOURCES,"tools/m2tgprecomp/main.cpp","src/runtime/tgp.h","src/runtime/tgp.cpp",
    "src/runtime/cpu.h","src/runtime/cpu.cpp","src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h","src/refcore/i960_ref.cpp"]))


def segment_rows(events,vehicle_events,marker):
    """Return seven-word selector/X/Z/progression/result rows and identities.

    Captured current is the actual stored signed16 encoding before the caller's
    clamp. The unused input current, point Y and asset-record prefix are not
    invented inputs to the native predicate.
    """
    rows,provenance=[],[]
    for group in motion_envelopes(events,vehicle_events,marker):
        pre,post=group[:2]
        before,after=pre["object_words"],post["object_words"]
        if any(before[i]!=after[i] for i in range(64) if i!=34):
            raise ValueError("Segment selection changed unrelated object storage")
        selector=block(pre,"course_selector",0x214354,1)[0]
        if selector>=8 or block(post,"course_selector",0x214354,1)!=[selector]:
            raise ValueError("Segment selector differs or escapes the verified descriptor window")
        x,z=before[5],before[7]
        if (x&0x7fffffff)>0x49800000 or (z&0x7fffffff)>0x49800000:
            raise ValueError("Segment point is outside the native arithmetic domain")
        selected=post["registers"][16]
        current,progression=after[34]&0xffff,after[34]>>16
        if selected!=0xffffffff and selected>=65535 or current!=(selected&0xffff) or \
           selected==0xffffffff and progression!=0xffff:
            raise ValueError("Segment return and stored index encodings disagree")
        rows.append((selector,x,z,before[34]>>16,selected,current,progression))
        provenance.append({"call_id":pre["call_id"],"frame":pre["frame"],"object":pre["object"],
            "instructions":post["instructions"]-pre["instructions"],"selector":selector,
            "matched":selected!=0xffffffff})
    return rows,provenance


def read_capture(directory):
    events,vehicle_events,marker,hashes=read_motion_capture(directory)
    rows,provenance=segment_rows(events,vehicle_events,marker)
    return rows,provenance,hashes


def asset_metadata(program,data):
    """Validate the private descriptor/index origins; export only extent hashes."""
    records=[]
    def region(address,size):
        offset=address-0x2000000
        if address&3 or offset<0 or size<0 or offset+size>len(data):
            raise ValueError("Segment asset extent leaves verified main data")
        return data[offset:offset+size]
    for selector in range(8):
        pointer=struct.unpack_from("<I",program,0x3dac8+12*selector)[0]
        header=region(pointer,12)
        count,vertices,segments=struct.unpack("<3I",header)
        if not 1<=count<=65535: raise ValueError("Segment asset count outside native policy")
        raw=region(segments,12*count)
        indices=[index for i in range(count) for index in struct.unpack_from("<4H",raw,12*i+4)]
        vertex_count=max(indices)+1
        coordinates=region(vertices,12*vertex_count)
        for index in set(indices):
            x,_,z=struct.unpack_from("<3I",coordinates,12*index)
            if max(x&0x7fffffff,z&0x7fffffff)>0x49800000:
                raise ValueError("Segment asset used vertex outside native policy")
        records.append({"selector":selector,"segments":count,"vertex_extent":vertex_count,
            "header_address":pointer,"vertices_address":vertices,"segments_address":segments,
            "header_sha256":hashlib.sha256(header).hexdigest(),
            "vertices_sha256":hashlib.sha256(coordinates).hexdigest(),
            "segments_sha256":hashlib.sha256(raw).hexdigest()})
    return records


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program",type=Path,required=True)
    parser.add_argument("--tgp-program",type=Path,required=True)
    parser.add_argument("--main-data",type=Path)
    parser.add_argument("--capture",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    inputs=[(args.program,2097152,PROGRAM_SHA),(args.tgp_program,6660,FIRMWARE_SHA)]
    try:
        if bool(args.capture)!=bool(args.main_data): raise ValueError("Live comparison needs both capture and main-data")
        if args.main_data: inputs.append((args.main_data,37748736,DATA_SHA))
        for path,size,expected in inputs:
            if path.stat().st_size!=size or digest(path)!=expected: raise ValueError("Private input differs: "+str(path))
        program=args.program.read_bytes()
        for start,end,expected in REGIONS:
            if hashlib.sha256(program[start:end]).hexdigest()!=expected: raise ValueError("Segment CPU region differs")
        start,end,expected=EDGE_REGION
        if hashlib.sha256(args.tgp_program.read_bytes()[4*start:4*end]).hexdigest()!=expected:
            raise ValueError("Segment edge firmware region differs")
        assets=asset_metadata(program,args.main_data.read_bytes()) if args.main_data else []
        rows,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],{})
        output=private_output(args.output)
    except (ValueError,OSError) as error: parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","segment-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(args.tgp_program.resolve()),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),
            *(str(ROOT/name) for name in ("platform/ps2/native/segment_selection.cpp",
                "platform/ps2/native/contact_projection.cpp","platform/ps2/tests/segment_selection_oracle.cpp",
                "src/runtime/cpu.cpp","src/refcore/i960_ref.cpp","src/runtime/tgp.cpp")),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,
        "main_data_sha256":DATA_SHA if assets else None,"cpu_regions":REGIONS,"edge_region":EDGE_REGION,
        "sources_sha256":initial,"commands":commands,"capture_sha256":capture_hashes,"assets":assets,
        "live_records":len(rows),"live_unique_inputs":len({row[:4] for row in rows}),"live_pairs":provenance,
        "live_comparison_verified":False,"physical_console_verified":False,"native_gameplay_connected":False,
        "scope":"Complete bounded segment search and projected quad predicate; subsequent caller clamp excluded"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(args.program.resolve()),str(args.tgp_program.resolve())]
        if rows:
            records=output/"private-segments.bin"
            records.write_bytes(b"SEGMENT1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<7I",*row) for row in rows))
            report["records_sha256"]=digest(records);run += [str(args.main_data.resolve()),str(records)]
        result=subprocess.run(run,capture_output=True,text=True,timeout=90)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),
            generated_sha256=digest(generated),generator_sha256=digest(generator))
        result.check_returncode()
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith("SEGMENT_SELECTION_ORACLE PASS ") or \
           f" live={len(rows)} " not in result.stdout:
            raise RuntimeError("Segment comparison lacks exact completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or \
           any(digest(path)!=expected for path,_,expected in inputs):
            raise RuntimeError("Segment comparison inputs changed")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows)
        print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__": main()
