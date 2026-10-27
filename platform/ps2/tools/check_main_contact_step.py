#!/usr/bin/env python3
"""Compare joined candidate/contact native state with sealed original callers."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from check_candidate_contact_pass import NATIVE as CONTACT_NATIVE, SOURCES as CONTACT_SOURCES, candidate_rows, verify_inputs
from check_contact_count import IDENTITIES
from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block, saved_return
from check_main_candidate import prefix_rows
from main_vehicle_capture import SOURCES as VEHICLE_SOURCES, read_vehicle_capture, vehicle_envelopes

NATIVE=("main_contact_step","main_candidate",*CONTACT_NATIVE)
SOURCES=list(dict.fromkeys([f"platform/ps2/native/{name}.{ext}" for name in NATIVE for ext in ("cpp","h")]+[
    "platform/ps2/tests/main_contact_step_live.cpp","platform/ps2/tests/main_contact_step_cases.h",
    "platform/ps2/tests/main_candidate_cases.h","platform/ps2/tests/contact_pass_response_cases.h",
    "platform/ps2/tools/check_main_contact_step.py","platform/ps2/tools/check_main_candidate.py",
    *VEHICLE_SOURCES,*[name for name in CONTACT_SOURCES if name.endswith(".py")]]))


def step_rows(vehicle_events,contact_events,marker,local_words):
    prefixes,prefix_provenance=prefix_rows(vehicle_events,marker)
    parents=vehicle_envelopes(vehicle_events,marker)
    candidates,candidate_provenance=candidate_rows(contact_events,marker,local_words)
    contact_groups=[];group=[]
    for event in contact_events:
        group.append(event)
        if event["event"]=="main_return": contact_groups.append(group);group=[]
    if group or len(contact_groups)!=len(parents) or len(candidates)!=len(prefixes):
        raise ValueError("Candidate/contact populations cannot be joined one-to-one")
    rows,provenance=[],[]
    for parent,contact,prefix,candidate,prefix_id,contact_id in zip(
            parents,contact_groups,prefixes,candidates,prefix_provenance,candidate_provenance):
        pre,prepared=parent[1:3];entry,returned=contact[0],contact[-1]
        fp=parent[0]["vehicle_frame_pointer"]
        if prepared["object"]!=entry["object"] or prepared["frame"]!=entry["frame"] or \
           entry["instructions"]!=prepared["instructions"]+1 or \
           not entry["instructions"]<returned["instructions"]<parent[3]["instructions"] or \
           returned["frame"]>parent[-1]["frame"] or saved_return(entry)!=(0x5d421c,fp) or \
           returned["registers"][31]!=fp:
            raise ValueError("Contact pass does not immediately follow its candidate caller")
        saved=prepared["registers"][:16].copy();saved[2]=0x5d421c
        if entry["return_frame"]["words"]!=saved or entry["registers"][16:31]!=prepared["registers"][16:31] or \
           entry["object_words"][:64]!=prepared["object_words"]:
            raise ValueError("Contact entry does not preserve exact candidate caller state")
        for name,address in (("candidate_position",fp+0x130),("candidate_orientation",fp+0x140)):
            if block(prepared,name,address,3)!=block(entry,name,address,3):
                raise ValueError("Joined candidate storage differs across the call")
        # The prefix's inspected stores touch only its object, candidate stacks
        # and marker. These regions must not alias the prior contact/feedback
        # state that is first observed at contact entry.
        destinations=((prepared["object"],0x100),(fp+0x130,12),(fp+0x140,12),(0x213974,4))
        preserved=((0x2138d0,144),(0x213960,12))
        if any(a<b+n and b<a+m for a,m in destinations for b,n in preserved):
            raise ValueError("Candidate stores alias contact/feedback provenance")
        before=list(prefix[:22])+list(candidate[48:52])+list(candidate[79:99])
        after_car=list(returned["object_words"][5:20])+[prefix[37]]
        after_car+=block(returned,"candidate_position",fp+0x130,3)
        after_car+=block(returned,"candidate_orientation",fp+0x140,3)
        after=after_car+list(candidate[61:65])+list(candidate[:20])
        inputs=list(candidate[65:79])+list(candidate[32:35])+[candidate[38]]
        row=tuple(before+after+inputs)
        if len(row)!=110: raise AssertionError("Joined step record width")
        rows.append(row)
        provenance.append({"vehicle_call_id":prefix_id["call_id"],"contact_call_id":contact_id["call_id"],
            "object":entry["object"],"entry_frame":pre["frame"],"exit_frame":returned["frame"],
            "instructions":returned["instructions"]-pre["instructions"],"call_transition_instructions":1,
            "selected_scalar":candidate[64],"contact_mask":candidate[63],
            "final_marker_source":"prefix_post plus inspected contact-callee preservation; not sampled at return",
            "initial_selected_scalar_source":"zero reset value; not a sampled input"})
    return rows,provenance


def read_capture(directory,local_words):
    directory=private_output(Path(directory))
    vehicle,marker,hashes=read_vehicle_capture(directory)
    path=directory/"main-contact.jsonl";private_output(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size>16*1024*1024:
        raise ValueError("Joined contact stream is missing, redirected or oversized")
    hashes[str(path)]=digest(path)
    validation=json.loads((directory.parent/"validation.json").read_text())
    if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:
        raise ValueError("Joined contact stream differs from its sealed identity")
    events=[json.loads(line) for line in path.read_text().splitlines()]
    rows,provenance=step_rows(vehicle,events,marker,local_words)
    if any(digest(Path(name))!=expected for name,expected in hashes.items()):
        raise ValueError("Joined observation inputs changed during parsing")
    return rows,provenance,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("program","tables","copro","capture","output"): parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args()
    paths={"program":args.program.resolve(),"tgp_tables":args.tables.resolve(),"copro_data":args.copro.resolve()}
    try:
        local_words=verify_inputs(paths);output=private_output(args.output)
        rows,provenance,capture_hashes=read_capture(args.capture,local_words)
    except (OSError,ValueError) as error: parser.error(str(error))
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    initial={name:digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True,exist_ok=False)
    executable,records=output/"main-contact-step-live",output/"private-steps.bin"
    command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off",
             "-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/tests/main_contact_step_live.cpp"),
             *(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE),"-o",str(executable)]
    report={"schema":1,"status":"failed","scope":"Joined candidate preparation and its immediately called contact pass",
        "native_gameplay_connected":False,"physical_console_verified":False,"live_comparison_verified":False,
        "sources_sha256":initial,"input_sha256":{name:IDENTITIES[name][1] for name in paths},
        "input_paths":{name:str(path) for name,path in paths.items()},"capture_sha256":capture_hashes,
        "command":command,"live_steps":len(rows),"live_pairs":provenance,
        "record_layout":{"words":110,"before":46,"after":46,"inputs":18,
                         "derived_after_word":15,"derived_initial_word":25,
                         "after_marker_directly_captured":False,"initial_selected_scalar_directly_captured":False},
        "limitations":["Sealed replay window, not a complete main callback or driving loop",
                       "Same-reference original evidence; no physical arcade arithmetic certification",
                       "Checker links no original CPU, generated firmware or device runtime"]}
    try:
        built=subprocess.run(command,capture_output=True,text=True,timeout=120)
        (output/"build.log").write_text(built.stdout+built.stderr);built.check_returncode()
        records.write_bytes(b"MAINSTP1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<110I",*row) for row in rows))
        report["records_sha256"]=digest(records)
        tested=subprocess.run([str(executable),*(str(path) for path in paths.values()),str(records)],capture_output=True,text=True,timeout=60)
        (output/"comparison.log").write_text(tested.stdout+tested.stderr)
        report.update(result=tested.stdout.strip(),exit_code=tested.returncode,executable_sha256=digest(executable))
        tested.check_returncode()
        match=re.fullmatch(r"MAIN_CONTACT_STEP_LIVE PASS steps=(\d+) contacts=(\d+) active_steps=(\d+) state_words=(\d+) hash=([0-9a-f]{16})\n?",tested.stdout)
        if match is None or tuple(map(int,(match[1],match[2],match[4])))!=(len(rows),4*len(rows),46*len(rows)) or int(match[3])>len(rows):
            raise RuntimeError("Joined checker lacks exact completion/population marker")
        if initial!={name:digest(ROOT/name) for name in SOURCES} or \
           any(digest(Path(name))!=expected for name,expected in capture_hashes.items()) or \
           any(digest(path)!=IDENTITIES[name][1] for name,path in paths.items()) or digest(records)!=report["records_sha256"]:
            raise RuntimeError("Joined comparison source or private inputs changed")
        report.update(status="passed",live_comparison_verified=True,active_steps=int(match[3]))
        print(tested.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__": main()
