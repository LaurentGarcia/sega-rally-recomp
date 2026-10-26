#!/usr/bin/env python3
"""Compare a bounded native slot update with private original CPU/geometry code."""
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
TABLE_SHA = "3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04"
REGION = (0x37200, 0x37678, "0fc800954def0406a250625a02b4ec97b124d6e81a1ea47933c907c35ee92e65")
SOURCES = list(dict.fromkeys([
    "platform/ps2/native/main_contact_slot.h", "platform/ps2/native/main_contact_slot.cpp",
    "platform/ps2/native/recovered_scalar_metric.h", "platform/ps2/native/recovered_scalar_metric.cpp",
    "platform/ps2/native/contact_result.h", "platform/ps2/native/contact_search.h",
    "platform/ps2/native/contact_projection.h", "platform/ps2/native/contact_neighborhood.h",
    "platform/ps2/tests/main_contact_slot_cases.h", "platform/ps2/tests/main_contact_slot_oracle.cpp",
    "platform/ps2/tools/check_main_contact_slot.py", *VEHICLE_SOURCES,
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp",
    "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]))


def slot_rows(events, marker):
    """Bind all observed slot inputs/outputs after the strict parent proof.

    Each row contains 43 input/state words and 26 result words. Arithmetic
    rejection belongs to the native/original comparator, never a lossy parser.
    """
    rows, provenance = [], []
    for group in vehicle_envelopes(events, marker):
        for index in range(4):
            pre, post = group[3+2*index:5+2*index]
            regs = pre["registers"]
            immutable = [("slot_vector",regs[17],3), ("slot_triple",regs[18],3),
                         ("slot_direction",regs[19],3), ("slot_result",0x2138d0+36*index,9),
                         ("slot_parameters",0x2141d0,5), ("slot_control",0x214124,1)]
            fields = {}
            for name,address,count in immutable:
                fields[name] = block(pre,name,address,count)
                if block(post,name,address,count) != fields[name]:
                    raise ValueError("Slot helper changed an immutable input block: "+name)
            before, after = pre["object_words"], post["object_words"]
            if any(before[i]!=after[i] for i in range(64) if i not in (4,25+index)) or \
               (before[4]&65535)!=(after[4]&65535):
                raise ValueError("Slot helper changed unrelated object storage")
            first = block(pre,"slot_globals",0x213880,16)
            last = block(post,"slot_globals",0x213880,16)
            if any(first[i]!=last[i] for i in range(16) if i%4!=index):
                raise ValueError("Slot helper changed another slot's global state")
            if post["registers"][16] not in (0,1) or post["registers"][30] or \
               pre["registers"][24:29] != post["registers"][24:29]:
                raise ValueError("Slot return flag or preserved registers differ")
            result = fields["slot_result"]
            inputs = [index,result[7],result[6],*result[3:6],*fields["slot_vector"],*fields["slot_direction"],
                      fields["slot_triple"][1],regs[20],before[20],*fields["slot_parameters"],fields["slot_control"][0]]
            old_state = [before[4]>>16,block(pre,"slot_mask",0x214208,1)[0],*before[25:29],
                         *first[8:12],*first[12:16],*first[4:8],*first[0:4]]
            output = [after[4]>>16,block(post,"slot_mask",0x214208,1)[0],*after[25:29],
                      *last[8:12],*last[12:16],*last[4:8],*last[0:4],
                      *block(post,"slot_output",regs[21],3),post["registers"][16]]
            row=tuple(inputs+old_state+output)
            if len(row)!=69: raise AssertionError("Slot row width")
            rows.append(row)
            provenance.append({"call_id":pre["call_id"],"frame":pre["frame"],"slot":index,
                "object":pre["object"],"active":bool(output[-1]),"status":result[7],
                "instructions":post["instructions"]-pre["instructions"]})
    return rows, provenance


def read_capture(directory):
    events,marker,hashes=read_vehicle_capture(directory)
    rows,provenance=slot_rows(events,marker)
    return rows,provenance,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program",type=Path,required=True)
    parser.add_argument("--tgp-program",type=Path,required=True)
    parser.add_argument("--tables",type=Path)
    parser.add_argument("--capture",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    if bool(args.capture)!=bool(args.tables): parser.error("--capture and --tables must be supplied together")
    images=[(args.program,2097152,PROGRAM_SHA),(args.tgp_program,6660,FIRMWARE_SHA)]
    if args.tables: images.append((args.tables,262144,TABLE_SHA))
    try:
        for path,size,expected in images:
            if path.stat().st_size!=size or digest(path)!=expected: raise ValueError("private input differs: "+str(path))
        start,end,expected=REGION
        if hashlib.sha256(args.program.read_bytes()[start:end]).hexdigest()!=expected:
            raise ValueError("slot helper instruction range differs")
        output=private_output(args.output)
        rows,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],{})
    except (ValueError,OSError) as error: parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","slot-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(args.tgp_program.resolve()),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),
            *(str(ROOT/name) for name in ("platform/ps2/native/main_contact_slot.cpp",
                "platform/ps2/native/recovered_scalar_metric.cpp","platform/ps2/tests/main_contact_slot_oracle.cpp",
                "src/runtime/cpu.cpp","src/refcore/i960_ref.cpp","src/runtime/tgp.cpp")),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,
        "table_sha256":TABLE_SHA if rows else None,"region":REGION,"sources_sha256":initial,"commands":commands,
        "capture_sha256":capture_hashes,"live_records":len(rows),"live_active":sum(row[-1] for row in rows),
        "live_unique_inputs":len({row[:43] for row in rows}),"live_pairs":provenance,
        "live_comparison_verified":False,"physical_console_verified":False,"native_gameplay_connected":False,
        "scope":"Bounded complete four-slot helper, including verified geometry58/2f reference; no full callback or driving-loop claim"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(args.program.resolve()),str(args.tgp_program.resolve())]
        if rows:
            records=output/"private-slots.bin"
            records.write_bytes(b"MCSLOT01"+struct.pack("<I",len(rows))+b"".join(struct.pack("<69I",*row) for row in rows))
            report["records_sha256"]=digest(records);run.extend((str(args.tables.resolve()),str(records)))
        result=subprocess.run(run,capture_output=True,text=True,timeout=90)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),
                      generated_sha256=digest(generated),generator_sha256=digest(generator))
        result.check_returncode()
        if len(result.stdout.splitlines())!=1 or not result.stdout.startswith("MAIN_CONTACT_SLOT_ORACLE PASS ") or \
           f" live={len(rows)} " not in result.stdout:
            raise RuntimeError("Slot comparison lacks exact live completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or \
           any(digest(path)!=expected for path,_,expected in images):
            raise RuntimeError("Slot source/private input changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows)
        print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__": main()
