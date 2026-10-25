#!/usr/bin/env python3
"""Validate bounded native contact accumulation against private CPU instructions."""
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

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
REGIONS = [(0x34b48,0x34b4c,"1576ac16c32d8c1e026cc53a61728e2724931abb39d34cda9feec68f71aa5cbc"),
           (0x34b5c,0x34b84,"98250fdd5ece906e4e9df98566239aae6e77aad7a0f2b00f08ecb79c3a1ab227"),
           (0x34b90,0x34cc0,"71e1c406f6c710ac7349859e7f738f1ff2a1929800653b712c49a1c5cb4ae017")]
SOURCES = ["platform/ps2/native/contact_aggregate.h", "platform/ps2/native/contact_aggregate.cpp",
    "platform/ps2/tests/contact_aggregate_cases.h", "platform/ps2/tests/contact_aggregate_oracle.cpp",
    "platform/ps2/tools/check_contact_aggregate.py", "platform/ps2/tools/check_contact_result.py",
    "platform/ps2/tools/check_contact_query.py", "platform/ps2/tools/check_contact_count.py",
    "platform/ps2/tools/check_contact_neighborhood.py", "platform/ps2/tools/check_driving_contact.py",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp", "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h"]


def aggregate_rows(events, marker):
    # This also verifies complete query/result parents, their dynamic returns
    # and the real pipelined ordering before consuming the later arithmetic.
    result_rows(events, marker, "main-contact")
    parent = pending_step = pending_update = None
    steps, updates, provenance = [], [], []
    for event in events:
        kind, regs = event["event"], event["registers"]
        if kind == "main_entry":
            position, orientation = regs[17:19]
            if not ordinary(position,12) or not ordinary(orientation,12):
                raise ValueError("Aggregate parent candidates require ordinary aligned RAM")
            parent = {"event": event, "steps": 0, "accumulator": 0, "update": False,
                      "position": position, "orientation": orientation}
            continue
        if kind == "main_return":
            if pending_step or pending_update or parent["steps"] not in (0,4) or parent["update"] != bool(parent["steps"]):
                raise ValueError("Incomplete aggregate parent")
            parent = None
            continue
        if kind not in ("metric_post", "step_post", "aggregate_pre", "aggregate_post"):
            if pending_step or pending_update:
                raise ValueError("Unexpected boundary inside aggregate arithmetic")
            continue
        if parent is None:
            raise ValueError("Aggregate event has no parent")
        fp = parent["event"]["main_frame_pointer"]
        proof = parent["event"]["return_frame"]
        if event.get("return_frame") != proof or saved_return(event) != (proof["pc"],proof["frame_pointer"]) or \
           regs[31] != fp or regs[13] != parent["event"]["object"] or integer(event.get("ac"),32) >> 30:
            raise ValueError("Aggregate parent, frame or nearest-even mode differs")
        position = block(event,"candidate_position",parent["position"],3)
        orientation = block(event,"candidate_orientation",parent["orientation"],3)
        responses = block(event,"response_vectors",fp+0xa0,12)
        feedback = block(event,"feedback",0x213960,3)
        enable = block(event,"feedback_enable",0x2142c8,1)
        immutable = (position,orientation,responses,feedback,enable,event["object_words"])
        if kind == "metric_post":
            index = integer(event.get("step_index"),32)
            if pending_step or pending_update or parent["update"] or index != parent["steps"] or index >= 4 or \
               event.get("metric_index") != index or regs[23] != 12*index or regs[30] != 0 or regs[11] != parent["accumulator"]:
                raise ValueError("Scalar accumulation index or preceding result differs")
            cross = block(event,"metric_cross",fp+0xd0,3)
            if regs[21] != cross[1]:
                raise ValueError("Scalar sign source differs from the captured cross component")
            sign = cross[1]
            if any((value & 0x7fffffff)>0x49800000 for value in (regs[11],regs[28],sign)) or \
               (regs[28] & 0x80000000 and regs[28] & 0x7fffffff):
                raise ValueError("Captured scalar input exceeds native domain")
            expected_cc = 2 if not sign & 0x7fffffff else (4 if sign & 0x80000000 else 1)
            if event["ac"] & 7 != expected_cc:
                raise ValueError("Saved scalar branch condition differs from its sign source")
            pending_step = (event,immutable,cross)
        elif kind == "step_post":
            if pending_step is None or pending_update or event.get("step_index") != parent["steps"] or regs[23] != 12*parent["steps"]:
                raise ValueError("Unpaired scalar accumulation result")
            before,old,cross = pending_step
            if immutable != old or block(event,"metric_cross",fp+0xd0,3) != cross or event["ac"] != before["ac"]:
                raise ValueError("Scalar accumulation changed memory or comparison state")
            steps.append((before["registers"][11],before["registers"][28],cross[1],regs[11]))
            provenance.append({"kind":"step","call_id":event["call_id"],"frame":event["frame"],"index":parent["steps"],
                               "instructions":event["instructions"]-before["instructions"]})
            parent["accumulator"] = regs[11]
            parent["steps"] += 1
            pending_step = None
        elif kind == "aggregate_pre":
            if pending_step or pending_update or parent["update"] or parent["steps"] != 4 or regs[11] != parent["accumulator"]:
                raise ValueError("Aggregate update lacks four completed scalar steps")
            words = event["object_words"]
            state = [words[i] for i in (11,13,18)] + position + orientation + feedback[1:]
            inputs = [regs[11]] + responses + [words[i] for i in (5,7,15,20,22)] + [int(enable[0]==0)]
            pending_update = (event,inputs,state,immutable)
        else:
            if pending_update is None or pending_step:
                raise ValueError("Unpaired aggregate update")
            before,inputs,state,old = pending_update
            words = event["object_words"]
            if responses != old[2] or enable != old[4] or feedback[0] != old[3][0] or regs[12] != before["registers"][12] or \
               any(words[i] != old[5][i] for i in range(256) if i not in (11,13,18)) or \
               position[1] != old[0][1] or orientation[0] != old[1][0] or orientation[2] != old[1][2] or \
               (enable[0] and feedback != old[3]):
                raise ValueError("Aggregate update modified preserved fields")
            output = [words[i] for i in (11,13,18)] + position + orientation + feedback[1:]
            updates.append(tuple(inputs+state+output))
            provenance.append({"kind":"update","call_id":event["call_id"],"frame":event["frame"],
                               "instructions":event["instructions"]-before["instructions"]})
            parent["update"] = True
            pending_update = None
    if parent or pending_step or pending_update or not steps or not updates or len(steps)!=4*len(updates):
        raise ValueError("Incomplete or empty aggregate capture")
    total_steps = integer(marker.get("main_step_entries"),64)
    total_updates = integer(marker.get("main_aggregate_entries"),64)
    if total_updates > integer(marker.get("main_calls"),64) or \
       integer(marker.get("main_step_completions"),64)!=total_steps or total_steps!=4*total_updates or \
       integer(marker.get("main_step_sampled_pairs"),64)!=len(steps) or len(steps)>total_steps or \
       integer(marker.get("main_aggregate_completions"),64)!=total_updates or \
       integer(marker.get("main_aggregate_samples"),64)!=len(updates) or len(updates)>total_updates:
        raise ValueError("Aggregate marker completion/sample counts differ")
    return steps,updates,provenance


def read_capture(directory):
    directory=private_output(Path(directory))
    paths=[directory/"capture.json",directory/"main-contact.jsonl",directory.parent/"validation.json"]
    for path,bound in zip(paths,(16384,64*1024*1024,1024*1024)):
        if path.stat().st_size>bound:raise ValueError("Aggregate capture exceeds bounded size")
    hashes={str(path):digest(path) for path in paths}
    marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or validation.get("status")!="passed" or \
       validation.get("exact_checkpoint_match") is not True:raise ValueError("Aggregate capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:raise ValueError("Aggregate capture identity differs")
    steps,updates,provenance=aggregate_rows([json.loads(line) for line in paths[1].read_text().splitlines()],marker)
    return steps,updates,provenance,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program",type=Path,required=True)
    parser.add_argument("--capture",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    program=args.program.resolve()
    if program.stat().st_size!=2097152 or digest(program)!=PROGRAM_SHA:parser.error("program differs from verified private identity")
    image=program.read_bytes()
    for start,end,expected in REGIONS:
        if hashlib.sha256(image[start:end]).hexdigest()!=expected:parser.error("aggregate instruction span differs")
    steps,updates,provenance,capture_hashes=read_capture(args.capture) if args.capture else ([],[],[],{})
    output=private_output(args.output)
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler:parser.error("C++20 compiler required")
    hashes={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    executable=output/"contact-aggregate-oracle"
    command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/native"),
        *(str(ROOT/name) for name in ("platform/ps2/native/contact_aggregate.cpp","platform/ps2/tests/contact_aggregate_oracle.cpp",
                                     "src/runtime/cpu.cpp","src/refcore/i960_ref.cpp")),"-o",str(executable)]
    report={"schema":1,"status":"failed","scope":"Bounded scalar accumulation and already-active aggregate state update",
        "program_sha256":PROGRAM_SHA,"regions":REGIONS,"sources_sha256":hashes,"command":command,"capture_sha256":capture_hashes,
        "live_steps":len(steps),"live_updates":len(updates),"live_pairs":provenance,"physical_console_verified":False,
        "native_gameplay_connected":False,"live_comparison_verified":False}
    try:
        built=subprocess.run(command,capture_output=True,text=True,timeout=60)
        (output/"build.log").write_text(built.stdout+built.stderr);built.check_returncode()
        run=[str(executable),str(program)]
        if steps:
            records=output/"private-records.bin"
            records.write_bytes(b"AGGRREC1"+struct.pack("<II",len(steps),len(updates))+
                b"".join(struct.pack("<4I",*row) for row in steps)+b"".join(struct.pack("<41I",*row) for row in updates))
            report["records_sha256"]=digest(records);run.append(str(records))
        tested=subprocess.run(run,capture_output=True,text=True,timeout=60)
        (output/"comparison.log").write_text(tested.stdout+tested.stderr)
        report.update(result=tested.stdout.strip(),exit_code=tested.returncode,executable_sha256=digest(executable))
        tested.check_returncode()
        if "CONTACT_AGGREGATE_ORACLE PASS" not in tested.stdout or f"live_steps={len(steps)} live_updates={len(updates)} " not in tested.stdout:
            raise RuntimeError("Aggregate oracle lacks completion/count marker")
        if hashes!={name:digest(ROOT/name) for name in SOURCES} or capture_hashes!={name:digest(Path(name)) for name in capture_hashes} or digest(program)!=PROGRAM_SHA:
            raise RuntimeError("Aggregate source, image or capture changed during comparison")
        report["status"]="passed";report["live_comparison_verified"]=bool(steps)
        print(tested.stdout,end="")
    finally:(output/"result.json").write_text(json.dumps(report,indent=2)+"\n")

if __name__=="__main__":main()
