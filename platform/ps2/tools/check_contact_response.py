#!/usr/bin/env python3
"""Compare typed per-contact responses with original CPU and private live calls."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
from check_contact_neighborhood import ROOT, digest, private_output
from check_contact_result import result_rows, ordinary
from check_driving_contact import block, integer, saved_return

PROGRAM_SHA="bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
REGION_SHA="c9a1bab4ddc45d7701dd07c632abd980172de75e639ca1dda58be55df622f5c5"
SOURCES=["platform/ps2/native/contact_response.h","platform/ps2/native/contact_response.cpp",
    "platform/ps2/native/contact_result.h","platform/ps2/native/contact_search.h",
    "platform/ps2/native/contact_normalization.h","platform/ps2/native/contact_normalization.cpp",
    "platform/ps2/native/rounded_cross_product.h","platform/ps2/native/rounded_cross_product.cpp",
    "platform/ps2/tests/contact_response_cases.h","platform/ps2/tests/contact_response_oracle.cpp",
    "platform/ps2/tools/check_contact_response.py","platform/ps2/tools/check_contact_result.py",
    "platform/ps2/tools/check_contact_query.py","platform/ps2/tools/check_contact_count.py",
    "platform/ps2/tools/check_contact_neighborhood.py","platform/ps2/tools/check_driving_contact.py",
    "src/runtime/cpu.cpp","src/runtime/cpu.h","src/runtime/fast_paths.h","src/refcore/i960_ref.cpp","src/refcore/i960_ref.h"]


def response_rows(events,marker):
    result_rows(events,marker,"main-contact")
    parent=pending=None
    rows,provenance=[],[]
    for event in events:
        kind,regs=event["event"],event["registers"]
        if kind=="main_entry":
            if not ordinary(regs[17],12) or not ordinary(regs[18],12):raise ValueError("Response candidate pointers require ordinary RAM")
            parent={"event":event,"count":0,"scalar":0,"mask":0,"position":regs[17],"orientation":regs[18]}
            continue
        if kind=="main_return":
            if pending or parent["count"]!=4 or regs[16]!=parent["scalar"]:raise ValueError("Incomplete response parent or changed selected return")
            parent=None
            continue
        if kind not in ("response_pre","response_post"):
            if pending and kind not in ("normalization_pre","normalization_post"):
                raise ValueError("Unexpected boundary inside contact response")
            continue
        if parent is None:raise ValueError("Response has no complete parent")
        fp=parent["event"]["main_frame_pointer"];obj=parent["event"]["object"];proof=parent["event"]["return_frame"]
        index=integer(event.get("contact_index"),32)
        if index!=parent["count"] or index>=4 or regs[31]!=fp or regs[13]!=obj or regs[5]!=index or regs[6]!=12*index or \
           regs[15]!=36*index or regs[3]!=0x2138ec+36*index or regs[9]!=0x2138d0+36*index or \
           regs[10]!=0x2138e8+36*index or regs[7]!=fp+0xd0 or regs[14]!=fp+0xe0 or regs[25]!=fp+0xd8 or regs[30]!=0 or \
           event.get("return_frame")!=proof or saved_return(event)!=(proof["pc"],proof["frame_pointer"]) or \
           integer(event.get("ac"),32)>>30:
            raise ValueError("Response index, native input mapping or nearest-even mode differs")
        slots=block(event,"result_slots",0x2138d0,36)
        offsets=block(event,"transformed_offsets",fp+0x70,12)
        responses=block(event,"response_vectors",fp+0xa0,12)
        feedback=block(event,"feedback",0x213960,3)
        enable=block(event,"feedback_enable",0x2142c8,1)
        position=block(event,"candidate_position",parent["position"],3)
        orientation=block(event,"candidate_orientation",parent["orientation"],3)
        if kind=="response_pre":
            if pending or event["pc"]!=0x5d372c or regs[12]!=parent["scalar"] or feedback[0]!=parent["mask"]:
                raise ValueError("Response input lacks the preceding scalar/mask")
            slot=slots[9*index:9*index+9]
            if regs[20]!=slot[7]&255:raise ValueError("Response gate register differs from observed status byte")
            inputs=slot[3:6]+offsets[3*index:3*index+3]+[slot[6],regs[12],event["object_words"][20],event["object_words"][21],slot[7]]
            active=bool(slot[7]&128)
            if active and (any((word&0x7fffffff)>0x41800000 for word in inputs[:6]) or
                           any((word&0x7fffffff)>0x44800000 for word in inputs[6:8]) or
                           any(not 0x3a800000<=word<=0x47800000 for word in inputs[8:10])):
                raise ValueError("Active response input exceeds declared native bounds")
            pending={"event":event,"inputs":inputs,"active":active,"slots":slots,"offsets":offsets,"responses":responses,
                     "feedback":feedback,"enable":enable,"position":position,"orientation":orientation}
            continue
        if pending is None or event["pc"]!=0x5d3a30:raise ValueError("Unpaired response output")
        expected_mask=(pending["feedback"][0]+((1<<index) if pending["active"] else 0))&0xffffffff
        if slots!=pending["slots"] or offsets!=pending["offsets"] or enable!=pending["enable"] or \
           event["object_words"]!=pending["event"]["object_words"] or position!=pending["position"] or orientation!=pending["orientation"] or \
           feedback!=[expected_mask]+pending["feedback"][1:] or \
           any(responses[i]!=pending["responses"][i] for i in range(12) if not 3*index<=i<3*index+3):
            raise ValueError("Response changed preserved parent/input storage")
        output=responses[3*index:3*index+3]+[regs[12],int(pending["active"])]
        if not pending["active"] and output!=[0,0,0,parent["scalar"],0]:
            raise ValueError("Inactive response changed scalar or positive-zero result")
        rows.append(tuple(pending["inputs"]+output))
        provenance.append({"call_id":event["call_id"],"frame":event["frame"],"index":index,"active":pending["active"],
                           "instructions":event["instructions"]-pending["event"]["instructions"]})
        parent["scalar"]=regs[12];parent["mask"]=expected_mask;parent["count"]+=1;pending=None
    calls=integer(marker.get("main_calls"),64)
    if parent or pending or not rows or len(rows)>128 or len(rows)!=4*integer(marker.get("main_samples"),64) or \
       integer(marker.get("main_response_entries"),64)!=4*calls or integer(marker.get("main_response_completions"),64)!=4*calls or \
       integer(marker.get("main_response_sampled_pairs"),64)!=len(rows):
        raise ValueError("Response completion/sample counts differ")
    return rows,provenance


def read_capture(directory):
    directory=private_output(Path(directory))
    paths=[directory/"capture.json",directory/"main-contact.jsonl",directory.parent/"validation.json"]
    for path,bound in zip(paths,(16384,64*1024*1024,1024*1024)):
        if path.stat().st_size>bound:raise ValueError("Response capture exceeds bounded size")
    hashes={str(path):digest(path) for path in paths}
    marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or validation.get("status")!="passed" or \
       validation.get("exact_checkpoint_match") is not True:raise ValueError("Response capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:raise ValueError("Response capture identity differs")
    rows,provenance=response_rows([json.loads(line) for line in paths[1].read_text().splitlines()],marker)
    return rows,provenance,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program",type=Path,required=True);parser.add_argument("--capture",type=Path);parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();program=args.program.resolve()
    if program.stat().st_size!=2097152 or digest(program)!=PROGRAM_SHA:parser.error("program differs from verified private identity")
    if hashlib.sha256(program.read_bytes()[0x34728:0x34a30]).hexdigest()!=REGION_SHA:parser.error("response instruction region differs")
    rows,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],{})
    output=private_output(args.output);compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler:parser.error("C++20 compiler required")
    hashes={name:digest(ROOT/name) for name in SOURCES};output.mkdir(parents=True,exist_ok=False);executable=output/"contact-response-oracle"
    command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/native"),
        *(str(ROOT/name) for name in ("platform/ps2/native/contact_response.cpp","platform/ps2/native/contact_normalization.cpp",
          "platform/ps2/native/rounded_cross_product.cpp","platform/ps2/tests/contact_response_oracle.cpp","src/runtime/cpu.cpp","src/refcore/i960_ref.cpp")),"-o",str(executable)]
    report={"schema":1,"status":"failed","scope":"Bounded per-contact bit7 gate, response vector/gain and selected scalar",
        "program_sha256":PROGRAM_SHA,"region":[0x34728,0x34a30],"region_sha256":REGION_SHA,"sources_sha256":hashes,
        "command":command,"capture_sha256":capture_hashes,"live_records":len(rows),"live_pairs":provenance,
        "physical_console_verified":False,"native_gameplay_connected":False,"live_comparison_verified":False}
    try:
        built=subprocess.run(command,capture_output=True,text=True,timeout=60);(output/"build.log").write_text(built.stdout+built.stderr);built.check_returncode()
        run=[str(executable),str(program)]
        if rows:
            records=output/"private-records.bin";records.write_bytes(b"RESPONS1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<16I",*row) for row in rows))
            report["records_sha256"]=digest(records);run.append(str(records))
        tested=subprocess.run(run,capture_output=True,text=True,timeout=60);(output/"comparison.log").write_text(tested.stdout+tested.stderr)
        report.update(result=tested.stdout.strip(),exit_code=tested.returncode,executable_sha256=digest(executable));tested.check_returncode()
        if "CONTACT_RESPONSE_ORACLE PASS" not in tested.stdout or f" live={len(rows)} " not in tested.stdout:raise RuntimeError("Missing response oracle completion/count marker")
        if hashes!={name:digest(ROOT/name) for name in SOURCES} or capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or digest(program)!=PROGRAM_SHA:
            raise RuntimeError("Response source, image or capture changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows);print(tested.stdout,end="")
    finally:(output/"result.json").write_text(json.dumps(report,indent=2)+"\n")

if __name__=="__main__":main()
