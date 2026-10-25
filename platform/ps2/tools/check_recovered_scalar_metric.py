#!/usr/bin/env python3
"""Compare a bounded recovered scalar operation with verified private firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block, integer, saved_return

FIRMWARE_SHA256="68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
REGIONS=[(0x51c,0x530,"3ae2c280643488b6e1a161908b726c7e1bf4ec72d05d5bdfe2317a95b25a1bc2"),
         (0x1ab,0x1cb,"a11bf92c9aa55906b4113ff89a9cefe94555b8a63c9095c13b35e3180e350e57"),
         (0x13,0x5e,"ddc9fb4c5c2eeedd460ecc61f7507b6b84c73a09e3e24862a8ec9a30ab214e64")]
SOURCES=["platform/ps2/native/recovered_scalar_metric.h","platform/ps2/native/recovered_scalar_metric.cpp",
    "platform/ps2/tests/recovered_scalar_metric_cases.h","platform/ps2/tests/recovered_scalar_metric_oracle.cpp",
    "platform/ps2/tools/check_recovered_scalar_metric.py","platform/ps2/tools/check_contact_neighborhood.py",
    "platform/ps2/tools/check_driving_contact.py","tools/m2tgprecomp/main.cpp",
    "src/runtime/tgp.h","src/runtime/tgp.cpp","src/runtime/fast_paths.h"]


def verify_firmware(image):
    if len(image)!=6660 or hashlib.sha256(image).hexdigest()!=FIRMWARE_SHA256:
        raise ValueError("firmware differs from verified private identity")
    for start,end,expected in REGIONS:
        if hashlib.sha256(image[start*4:end*4]).hexdigest()!=expected:
            raise ValueError("metric command, primitive or startup region differs")


def ordinary(address,size):
    return not address&3 and ((0x200000<=address<=0x240000-size) or (0x500000<=address<=0x600000-size))


def metric_rows(events,marker):
    kinds={"main_entry","main_prequery","query_pre","query_count","query_post","contact_pre","contact_reply",
           "contact_post","response_post","normalization_pre","normalization_post","cross_pre","cross_post",
           "metric_post","step_post","aggregate_pre","aggregate_post","main_return"}
    parent=pending=None
    rows,pairs,seen=[],[],set()
    last_instruction=-1
    for event in events:
        if not isinstance(event,dict) or event.get("schema")!=1 or event.get("event") not in kinds:
            raise ValueError("Unsupported metric parent event/schema")
        kind=event["event"]
        call,obj=integer(event.get("call_id"),64),integer(event.get("object"),32)
        frame,instruction=integer(event.get("frame"),64),integer(event.get("instructions"),64)
        fp,pc=integer(event.get("main_frame_pointer"),32),integer(event.get("pc"),32)
        integer(event.get("ac"),32)
        regs,floats,words=event.get("registers"),event.get("fp_bits"),event.get("object_words")
        if not isinstance(regs,list) or len(regs)!=32 or not isinstance(floats,list) or len(floats)!=4 or \
           not isinstance(words,list) or len(words)!=256:
            raise ValueError("Metric parent requires complete register/object state")
        for value in regs+words:
            integer(value,32)
        for value in floats:
            integer(value,64)
        if not call or not ordinary(obj,1024) or not ordinary(fp,0x114) or fp&63 or instruction<=last_instruction:
            raise ValueError("Invalid metric identity, memory range or order")
        last_instruction=instruction
        if kind=="main_entry":
            if parent is not None or call in seen or len(seen)>=32 or pc!=0x5d3380 or regs[16]!=obj or regs[31]!=fp:
                raise ValueError("Nested, duplicate or invalid metric parent entry")
            if not isinstance(event.get("return_frame"),dict):
                raise ValueError("Metric parent lacks saved caller proof")
            return_pc,caller_fp=saved_return(event)
            if return_pc!=0x5d421c or event.get("parent_return_frame")!=event["return_frame"]:
                raise ValueError("Metric parent differs from verified caller")
            parent={"call":call,"object":obj,"fp":fp,"frame":frame,"proof":event["return_frame"],
                    "return_pc":return_pc,"caller_fp":caller_fp,"pairs":0}
            continue
        if parent is None or call!=parent["call"] or obj!=parent["object"] or fp!=parent["fp"] or \
           frame<parent["frame"] or event.get("parent_return_frame")!=parent["proof"]:
            raise ValueError("Metric event lacks unchanged parent identity")
        if kind=="main_return":
            if pending is not None or parent["pairs"] not in (0,4) or pc!=parent["return_pc"] or \
               regs[31]!=parent["caller_fp"] or event.get("return_frame")!=parent["proof"]:
                raise ValueError("Metric parent return is incomplete or differs from saved caller")
            seen.add(call);parent=None;continue
        if kind not in ("cross_post","metric_post"):
            if pending is not None:
                raise ValueError("Unexpected event within scalar metric pair")
            continue
        index=integer(event.get("metric_index"),32)
        if index>=4 or index!=parent["pairs"] or regs[31]!=fp or regs[13]!=obj or regs[23]!=12*index or \
           regs[30]!=0 or regs[16:19]!=[fp+0xd8,fp+0xd4,fp+0xd0] or event.get("return_frame")!=parent["proof"]:
            raise ValueError("Metric index/frame/zero/result pointers differ")
        if saved_return(event)!=(parent["return_pc"],parent["caller_fp"]):
            raise ValueError("Metric saved caller differs from parent")
        cross=block(event,"metric_cross",fp+0xd0,3)
        if kind=="cross_post":
            if pending is not None or pc!=0x5d3b04 or integer(event.get("cross_index"),32)!=index or \
               regs[21]!=fp+0x40+12*index or len(rows)>=16384:
                raise ValueError("Invalid or excessive metric input boundary")
            if any((value&0x7fffffff)>0x49800000 for value in cross):
                raise ValueError("Captured metric coordinates exceed native finite domain")
            pending={"cross":cross,"frame":frame,"instructions":instruction,"object_words":words}
            continue
        if pending is None or pc!=0x5d3b5c or cross!=pending["cross"] or frame<pending["frame"] or \
           words!=pending["object_words"] or regs[20:23]!=[cross[0],cross[1],cross[2]] or regs[29]!=0x2c005858:
            raise ValueError("Unpaired metric result or modified input/transport state")
        rows.append((0,0,cross[1],0,cross[0],cross[2],regs[28]))
        pairs.append({"call_id":call,"object":obj,"index":index,"entry_frame":pending["frame"],
                      "exit_frame":frame,"instructions":instruction-pending["instructions"]})
        parent["pairs"]+=1;pending=None
    if parent is not None or pending is not None or not rows:
        raise ValueError("Incomplete or empty live scalar metric capture")
    calls=integer(marker.get("main_calls"),64)
    total=integer(marker.get("main_metric_entries"),64)
    if not calls or integer(marker.get("main_completions"),64)!=calls or \
       integer(marker.get("main_samples"),64)!=len(seen) or not len(seen)<=calls or \
       integer(marker.get("main_events"),64)!=len(events) or total%4 or not len(rows)<=total<=4*calls or \
       integer(marker.get("main_metric_completions"),64)!=total or \
       integer(marker.get("main_metric_sampled_pairs"),64)!=len(rows) or \
       integer(marker.get("main_cross_pairs"),64)!=total or \
       integer(marker.get("main_sampled_cross_pairs"),64)!=len(rows):
        raise ValueError("Metric capture completion/sample counts differ")
    for name in ("main_queries","main_query_completions","main_contacts","main_contact_completions"):
        if integer(marker.get(name),64)!=4*calls:
            raise ValueError("Metric parent lacks four completed course queries/results")
    return rows,pairs


def capture_rows(directory):
    directory=private_output(Path(directory))
    paths=[directory/"capture.json",directory/"main-contact.jsonl",directory.parent/"validation.json"]
    for path,limit in zip(paths,(16384,32*1024*1024,1024*1024)):
        if path.stat().st_size>limit:
            raise ValueError("Metric capture exceeds bounded reader size")
    hashes={str(path):digest(path) for path in paths}
    marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or \
       validation.get("status")!="passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Metric capture lacks complete observer-parity validation")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:
            raise ValueError("Metric capture differs from validated identity")
    rows,pairs=metric_rows([json.loads(line) for line in paths[1].read_text().splitlines()],marker)
    return rows,pairs,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tgp-program",type=Path,required=True)
    parser.add_argument("--capture",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    firmware=args.tgp_program.resolve()
    try:
        verify_firmware(firmware.read_bytes());output=private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    rows,pairs,capture_hashes=capture_rows(args.capture) if args.capture else ([],[],{})
    output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","metric-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
        [str(generator),str(firmware),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),
            str(ROOT/"platform/ps2/tests/recovered_scalar_metric_oracle.cpp"),
            str(ROOT/"platform/ps2/native/recovered_scalar_metric.cpp"),str(ROOT/"src/runtime/tgp.cpp"),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","firmware_sha256":FIRMWARE_SHA256,"regions":REGIONS,
        "sources_sha256":initial,"commands":commands,"capture_sha256":capture_hashes,
        "live_records":len(rows),"live_unique_inputs":len({row[:6] for row in rows}),"live_pairs":pairs,
        "live_comparison_verified":False,"physical_console_verified":False,
        "scope":"Bounded recovered scalar metric; exact pinned reference, not correctly rounded Euclidean distance"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(firmware)]
        if rows:
            records=output/"metric-records.bin"
            with records.open("xb") as stream:
                stream.write(b"RXMETR1\0"+struct.pack("<I",len(rows)))
                for row in rows:
                    stream.write(struct.pack("<7I",*row))
            report["records_sha256"]=digest(records);run.append(str(records))
        result=subprocess.run(run,capture_output=True,text=True,timeout=60)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,
            executable_sha256=digest(executable),generated_sha256=digest(generated),generator_sha256=digest(generator))
        result.check_returncode()
        if "RECOVERED_METRIC_ORACLE PASS" not in result.stdout:
            raise RuntimeError("original metric comparison lacks completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or digest(firmware)!=FIRMWARE_SHA256 or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes}:
            raise RuntimeError("metric source or private input changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows)
        print(result.stdout,end="")
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__":
    main()
