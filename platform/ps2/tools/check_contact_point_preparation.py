#!/usr/bin/env python3
"""Source-bound original-firmware and sealed driving point-preparation comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block, integer, saved_return

PROGRAM_SHA256="bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA256="68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
PROGRAM_REGIONS=[(0x34380,0x346d8,"4298396bc5e5ca77d1b9c95485dabaaec2534c5adca65ac2f11a8a6fb117e776"),
    (0x33e30,0x33e60,"1f2b37b346c38c1a535c6aa8dd85af25d5897483480148ce5ce424b423aeda7a")]
FIRMWARE_REGIONS=[(0x13,0x5e,"ddc9fb4c5c2eeedd460ecc61f7507b6b84c73a09e3e24862a8ec9a30ab214e64"),
    (0x1e6,0x21d,"852c3c81157047d7b219d662648aec78eace62a3ef552630108302b850ebafcc"),
    (0x25b,0x27e,"5eead2e228c96affcebb961e03ce4726060ba7bbb56f62c2054a3b846b084ca0"),
    (0x327,0x32e,"381ec888e8452d6d6497c4f696b8304dfb8e7092a35ee74fca46ddd52d4ba240"),
    (0x363,0x380,"e94713e5d70c083e419ea4c547d474f1301f685829b428791c495e765d1d6670"),
    (0x384,0x3a0,"039150a47ecfc2b72392ed55accb85c4313ad7ef024d80c95bafd911f461509b"),
    (0x3a4,0x3c0,"37b95d34032853c91ffe37c864442ccfcf8fb70a6fd7b650fc4c51736b9ef4c0"),
    (0x404,0x414,"8d423dcd3c375cb5e11279ba77f87a942c1cc287a33a3c6e0fbafffdbd26572c"),
    (0x44c,0x46c,"229d95dd4c345dc0a0eda94ba6988aa858b20b18bb99143cad0753e93626b273")]
SOURCES=["platform/ps2/native/contact_point_preparation.h","platform/ps2/native/contact_point_preparation.cpp",
    "platform/ps2/native/rounded_affine_transform.h","platform/ps2/native/rounded_affine_transform.cpp",
    "platform/ps2/tests/contact_point_preparation_cases.h","platform/ps2/tests/contact_point_preparation_oracle.cpp",
    "platform/ps2/tools/check_contact_point_preparation.py","platform/ps2/tools/check_contact_neighborhood.py",
    "platform/ps2/tools/check_driving_contact.py","tools/m2tgprecomp/main.cpp",
    "src/runtime/tgp.h","src/runtime/tgp.cpp","src/runtime/fast_paths.h"]


def verify_images(program,firmware):
    if len(program)!=2097152 or hashlib.sha256(program).hexdigest()!=PROGRAM_SHA256:
        raise ValueError("point-preparation program identity differs")
    if len(firmware)!=6660 or hashlib.sha256(firmware).hexdigest()!=FIRMWARE_SHA256:
        raise ValueError("point-preparation firmware identity differs")
    for image,regions,scale in ((program,PROGRAM_REGIONS,1),(firmware,FIRMWARE_REGIONS,4)):
        for start,end,expected in regions:
            if hashlib.sha256(image[start*scale:end*scale]).hexdigest()!=expected:
                raise ValueError("point-preparation evidence interval differs")
    return struct.unpack("<12I",program[0x33e30:0x33e60])


def ordinary(address,size):
    return not address&3 and ((0x200000<=address<=0x240000-size) or (0x500000<=address<=0x600000-size))


def point_rows(events,marker,local_words):
    if len(local_words)!=12:
        raise ValueError("point preparation needs four verified local triples")
    for value in local_words:
        integer(value,32)
    kinds={"main_entry","main_prequery","query_pre","query_count","query_post","contact_pre","contact_reply",
        "contact_post","response_pre","response_post","normalization_pre","normalization_post","cross_pre","cross_post",
        "metric_post","step_post","aggregate_pre","aggregate_post","main_return"}
    parent=None;seen=set();rows=[];pairs=[];last_instruction=-1
    for event in events:
        if not isinstance(event,dict) or event.get("schema")!=1 or event.get("event") not in kinds:
            raise ValueError("unsupported point-preparation event/schema")
        kind=event["event"];call=integer(event.get("call_id"),64);obj=integer(event.get("object"),32)
        fp,pc=integer(event.get("main_frame_pointer"),32),integer(event.get("pc"),32)
        frame,instruction=integer(event.get("frame"),64),integer(event.get("instructions"),64)
        ac=integer(event.get("ac"),32)
        regs,floats,words=event.get("registers"),event.get("fp_bits"),event.get("object_words")
        if not isinstance(regs,list) or len(regs)!=32 or not isinstance(floats,list) or len(floats)!=4 or not isinstance(words,list) or len(words)!=256:
            raise ValueError("point-preparation requires complete raw state")
        for value in regs+words:integer(value,32)
        for value in floats:integer(value,64)
        if not call or not ordinary(obj,1024) or not ordinary(fp,0x114) or fp&63 or instruction<=last_instruction:
            raise ValueError("invalid point-preparation identity/range/order")
        last_instruction=instruction
        if kind=="main_entry":
            if parent is not None or call in seen or len(seen)>=32 or pc!=0x5d3380 or regs[16]!=obj or regs[31]!=fp or regs[30]!=0 or ac>>30:
                raise ValueError("invalid point-preparation entry")
            if not isinstance(event.get("return_frame"),dict):
                raise ValueError("point-preparation lacks saved caller proof")
            return_pc,caller_fp=saved_return(event)
            if return_pc!=0x5d421c or event.get("parent_return_frame")!=event["return_frame"] or not ordinary(regs[17],12) or not ordinary(regs[18],12):
                raise ValueError("point-preparation caller/input pointers differ")
            position=block(event,"candidate_position",regs[17],3)
            angles=block(event,"candidate_orientation",regs[18],3)
            local=block(event,"local_contact_points",0x5d2e30,12)
            if tuple(local)!=tuple(local_words) or any((v&0x7fffffff)>0x41000000 for v in angles) or any((v&0x7fffffff)>0x49800000 for v in position+local):
                raise ValueError("point-preparation local asset or finite domain differs")
            parent={"call":call,"object":obj,"fp":fp,"frame":frame,"instructions":instruction,
                "proof":event["return_frame"],"return_pc":return_pc,"caller_fp":caller_fp,
                "position_address":regs[17],"angle_address":regs[18],"position":position,"angles":angles,
                "local":local,"words":words,"done":False}
            continue
        if parent is None or call!=parent["call"] or obj!=parent["object"] or fp!=parent["fp"] or frame<parent["frame"] or event.get("parent_return_frame")!=parent["proof"]:
            raise ValueError("point event lacks unchanged parent identity")
        if kind=="main_prequery":
            if parent["done"] or pc!=0x5d36d8 or regs[31]!=fp or regs[13]!=obj or regs[30]!=0 or ac>>30 or regs[18]!=fp+0x40 or words!=parent["words"] or event.get("return_frame")!=parent["proof"]:
                raise ValueError("point-preparation output boundary/state differs")
            if saved_return(event)!=(parent["return_pc"],parent["caller_fp"]):
                raise ValueError("point-preparation saved caller changed")
            if block(event,"candidate_position",parent["position_address"],3)!=parent["position"] or block(event,"candidate_orientation",parent["angle_address"],3)!=parent["angles"] or block(event,"local_contact_points",0x5d2e30,12)!=parent["local"]:
                raise ValueError("point-preparation input changed")
            offsets=block(event,"transformed_offsets",fp+0x70,12);points=block(event,"query_points",fp+0x40,12)
            if any((v&0x7fffffff)>=0x7f800000 for v in offsets+points):
                raise ValueError("nonfinite prepared output")
            rows.append(tuple(parent["angles"]+parent["position"]+parent["local"]+offsets+points))
            pairs.append({"call_id":call,"object":obj,"entry_frame":parent["frame"],"exit_frame":frame,"instructions":instruction-parent["instructions"]})
            parent["done"]=True;continue
        if not parent["done"]:
            raise ValueError("unexpected child event inside point-preparation interval")
        if kind=="main_return":
            if pc!=parent["return_pc"] or regs[31]!=parent["caller_fp"] or event.get("return_frame")!=parent["proof"]:
                raise ValueError("point-preparation enclosing return differs")
            seen.add(call);parent=None
    if parent is not None or not rows or len(rows)!=len(seen):
        raise ValueError("incomplete/empty point-preparation capture")
    calls=integer(marker.get("main_calls"),64)
    if not calls or calls<len(rows) or integer(marker.get("main_completions"),64)!=calls or integer(marker.get("main_samples"),64)!=len(rows) or integer(marker.get("main_events"),64)!=len(events):
        raise ValueError("point-preparation completion/sample counts differ")
    for name in ("main_queries","main_query_completions","main_contacts","main_contact_completions"):
        if integer(marker.get(name),64)!=4*calls:
            raise ValueError("point-preparation parent completion count differs")
    return rows,pairs


def capture_rows(directory,local_words):
    directory=private_output(Path(directory));paths=[directory/"capture.json",directory/"main-contact.jsonl",directory.parent/"validation.json"]
    for path,limit in zip(paths,(16384,32*1024*1024,1024*1024)):
        if path.stat().st_size>limit:raise ValueError("point capture exceeds bounded reader size")
    hashes={str(path):digest(path) for path in paths};marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or validation.get("status")!="passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("point capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:raise ValueError("point capture differs from validated identity")
    rows,pairs=point_rows([json.loads(line) for line in paths[1].read_text().splitlines()],marker,local_words)
    return rows,pairs,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--program",type=Path,required=True);parser.add_argument("--tgp-program",type=Path,required=True);parser.add_argument("--capture",type=Path);parser.add_argument("--output",type=Path,required=True);args=parser.parse_args()
    try:
        program,firmware=args.program.resolve(),args.tgp_program.resolve()
        local_words=verify_images(program.read_bytes(),firmware.read_bytes());output=private_output(args.output)
        rows,pairs,capture_hashes=capture_rows(args.capture,local_words) if args.capture else ([],[],{})
    except (ValueError,OSError) as error:parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler:parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES};output.mkdir(parents=True,exist_ok=False)
    generator,generated,executable=(output/name for name in ("m2tgprecomp","private-tgp.cpp","point-oracle"))
    common=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src")]
    commands=[common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],[str(generator),str(firmware),str(generated)],
        common+["-DM2TGP_WITH_HOOK","-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/tests/contact_point_preparation_oracle.cpp"),str(ROOT/"platform/ps2/native/contact_point_preparation.cpp"),str(ROOT/"platform/ps2/native/rounded_affine_transform.cpp"),str(ROOT/"src/runtime/tgp.cpp"),str(generated),"-o",str(executable)]]
    report={"schema":1,"status":"failed","program_sha256":PROGRAM_SHA256,"firmware_sha256":FIRMWARE_SHA256,"program_regions":PROGRAM_REGIONS,"firmware_regions":FIRMWARE_REGIONS,"sources_sha256":initial,"commands":commands,"capture_sha256":capture_hashes,"live_records":len(rows),"live_unique_inputs":len({row[:18] for row in rows}),"live_pairs":pairs,"live_comparison_verified":False,"physical_console_verified":False,"scope":"Bounded original orientation polynomial/matrix and four prepared contact points; no generic trigonometry or physical fidelity claim"}
    try:
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=60);(output/f"build-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        run=[str(executable),str(firmware)]
        if rows:
            records=output/"point-records.bin"
            with records.open("xb") as stream:
                stream.write(b"RXPOIN1\0"+struct.pack("<I",len(rows)))
                for row in rows:stream.write(struct.pack("<42I",*row))
            report["records_sha256"]=digest(records);run.append(str(records))
        result=subprocess.run(run,capture_output=True,text=True,timeout=90);(output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(executable),generated_sha256=digest(generated),generator_sha256=digest(generator));result.check_returncode()
        if "CONTACT_POINT_ORACLE PASS" not in result.stdout:raise RuntimeError("point oracle lacks completion marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or digest(program)!=PROGRAM_SHA256 or digest(firmware)!=FIRMWARE_SHA256 or capture_hashes!={name:digest(Path(name)) for name in capture_hashes}:raise RuntimeError("point source/private input changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(rows);print(result.stdout,end="")
    finally:(output/"result.json").write_text(json.dumps(report,indent=2)+"\n")

if __name__=="__main__":main()
