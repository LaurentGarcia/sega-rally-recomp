#!/usr/bin/env python3
"""Extract entry/front stage fixtures from the sealed continuous CPU observation."""
import argparse
import json
from pathlib import Path
import struct

from check_contact_neighborhood import ROOT,digest,private_output
from full_step_capture import SOURCES as READER_SOURCES,blocks,offset,read_full_step_capture,replay_steps
from extract_main_motion import compact_record

SOURCES=["platform/ps2/tools/extract_main_stages.py","platform/ps2/tools/extract_main_motion.py",*READER_SOURCES]
ENTRY_REQUIRED=((0x2020a8,1),(0x2139d8,1),(0x2142c0,1),(0x2140c4,2),
                (0x20a560,1),(0x202050,1),(0x214354,1))
FRONT_REQUIRED=((0x213880,16),(0x2138d0,36),(0x213960,6),(0x214208,1),
                (0x2141d0,5),(0x214354,1),(0x214120,2),(0x214200,2),
                (0x214270,26),(0x214258,4),(0x20b0b4,1))


def initial(event,memory,extra):
    state={"event":event,"before":bytes(memory),"after":None,"addresses":set(),"selected":None}
    spans=[(event["object"],64),(event["vehicle_frame_pointer"],0x1f4//4),*extra]
    spans += [(a,len(v)) for (name,a),v in blocks(event).items()]
    for address,count in spans: state["addresses"].update(range(address,address+4*count,4))
    return state


def extract(groups,summary,paths,entry_stream,front_stream):
    entry_stream.write(b"ENTSTEP1"+struct.pack("<I",len(groups)))
    front_stream.write(b"FRONTST1"+struct.pack("<I",len(groups)))
    parent=None;entry=None;front=None;proofs=[]
    def boundary(event,memory):
        nonlocal parent,entry,front
        if event["event"]=="vehicle_entry":
            parent=event;entry=initial(event,memory,ENTRY_REQUIRED);front=None
        elif event["event"]=="balance_helper_pre":
            if front is None or front["after"] is not None:
                raise ValueError("Front stage lacks a unique start before the balance helper")
            front["after"]=bytes(memory);front["end_instructions"]=event["instructions"]
    def transaction(record,memory):
        nonlocal front
        instruction,pc,address,value,flags=record
        if flags&0x200: return
        if entry is not None and entry["after"] is None and pc==0x5d3ff0:
            entry["after"]=bytes(memory);entry["end_instructions"]=instruction
        if pc==0x5d4224 and front is None:
            if entry is None or entry["after"] is None: raise ValueError("Front start precedes completed entry")
            front=initial(parent,memory,FRONT_REQUIRED);front["start_instructions"]=instruction
        width=flags&255
        for stage in (entry,front):
            if stage is None or stage["after"] is not None: continue
            if offset(address,width) is not None and address<0x5a0000:
                stage["addresses"].update(range(address&~3,(address+width+3)&~3,4))
        if entry is not None and entry["after"] is None and flags&0x100 and address<0x20b180 and address+width>0x20b100:
            raise ValueError("Entry unexpectedly writes the sound queue")
        if front is not None and front["after"] is None and pc==0x5d46bc and flags&0x100:
            if width!=4 or address!=parent["vehicle_frame_pointer"]+0x1a0 or front["selected"] is not None:
                raise ValueError("Front selected scalar write has unexpected identity")
            front["selected"]=value
    for step in replay_steps(groups,summary,paths["full-step-seed.bin"],paths["full-step-access.bin"],
                             on_boundary=boundary,on_transaction=transaction):
        if entry is None or front is None or entry["after"] is None or front["after"] is None or front["selected"] is None:
            raise ValueError("Main stage is missing a complete original boundary pair or selected result")
        fp,obj=parent["vehicle_frame_pointer"],parent["object"]
        entry_bytes=compact_record(step["frame"],fp,obj,entry["before"],entry["after"],entry["addresses"],[])
        front_bytes=compact_record(step["frame"],fp,obj,front["before"],front["after"],front["addresses"],[])
        # FRONTST1 uses the final word for the preceding contact pass's scalar;
        # it has no queue field. This is an isolated stage input, never a
        # replacement for producing that result in a native complete update.
        front_bytes=front_bytes[:-4]+struct.pack("<I",front["selected"])
        entry_stream.write(entry_bytes);front_stream.write(front_bytes)
        proofs.append({"frame":step["frame"],"entry_start":parent["instructions"],
                       "entry_end":entry["end_instructions"],"front_start":front["start_instructions"],
                       "front_end":front["end_instructions"],"selected_scalar":front["selected"],
                       "entry_bytes":len(entry_bytes),"front_bytes":len(front_bytes)})
    return proofs


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();output=private_output(args.output)
    if output.exists(): parser.error("stage output must be a new private directory")
    groups,summary,paths,hashes=read_full_step_capture(args.capture)
    initial_hashes={s:digest(ROOT/s) for s in SOURCES};output.mkdir(parents=True)
    report={"schema":1,"status":"failed","capture_sha256":hashes,"sources_sha256":initial_hashes,
            "scope":"Isolated entry and front stages from complete original callbacks; no native consecutive update claim"}
    try:
        with (output/"entry.bin.tmp").open("xb") as entry,(output/"front.bin.tmp").open("xb") as front:
            report["records"]=extract(groups,summary,paths,entry,front)
        if initial_hashes!={s:digest(ROOT/s) for s in SOURCES} or any(digest(Path(p))!=h for p,h in hashes.items()):
            raise ValueError("Stage sources or capture changed during extraction")
        for name in ("entry","front"):
            (output/(name+".bin.tmp")).rename(output/(name+".bin"));report[name+"_sha256"]=digest(output/(name+".bin"))
        report.update(status="passed",record_count=len(groups));print(json.dumps({k:report[k] for k in ("status","record_count","entry_sha256","front_sha256")}))
    except Exception as error: report["error"]=str(error);raise
    finally: (output/"result.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")


if __name__=="__main__": main()
