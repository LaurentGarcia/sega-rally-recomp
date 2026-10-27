#!/usr/bin/env python3
"""Extract one seed, explicit external inputs and expected complete callback outputs."""
import argparse
import json
from pathlib import Path
import struct
from check_contact_neighborhood import ROOT,digest,private_output
from full_step_capture import SOURCES as READER_SOURCES,offset,read_full_step_capture,replay_steps
from extract_main_motion import words,ranges
SOURCES=["platform/ps2/tools/extract_main_vehicle.py","platform/ps2/tools/extract_main_motion.py",*READER_SOURCES]
INPUTS=(0x202008,0x202044,0x202098,0x2020a8,0x214120,0x2020b4,0x2139cc,0x202050,0x2139d8,0x202018,0x2139d0,0x20a560)
SPANS=((0x202048,1),(0x20b0b4,1),(0x20b1a4,3),(0x213870,1),(0x213880,64),
       (0x2140c4,2),(0x2141c0,13),(0x214200,7),(0x214230,66),(0x2197b0,1))

def extract(groups,summary,paths,stream):
    stream.write(b"VEHSTEP1"+struct.pack("<II",len(groups),0x140000))
    current={};identity=None;invariant=None;rows=[];hist={hex(a):{} for a in INPUTS}
    def boundary(event,memory):
        nonlocal identity,invariant
        if event["event"]=="vehicle_entry":
            obj,fp=event["object"],event["vehicle_frame_pointer"]
            pose=words(memory,obj+0x8c,1)[0]
            if offset(pose,88) is None or pose&3: raise ValueError("Published pose is not bounded ordinary memory")
            this_identity=(obj,fp,pose)
            fixed=tuple(words(memory,obj+0x50,4))+tuple(words(memory,0x214124,1))+tuple(words(memory,0x214354,1))
            if identity is None:
                identity=this_identity;invariant=fixed;stream.write(memory)
            elif this_identity!=identity or fixed!=invariant: raise ValueError(f"Canonical object/storage/setup/course changed at frame {event['frame']}: identity {identity}/{this_identity}, fixed {invariant}/{fixed}")
            current.clear();current.update(event=event,pose=pose,queue=[],overflow=False,
                inputs=[words(memory,a,1)[0] for a in INPUTS],queue_cursor=words(memory,0x20b184,1)[0],reference=words(memory,0x214360,3),after=None)
            for a,value in zip(INPUTS,current["inputs"]):
                key=f"{value:08x}";hist[hex(a)][key]=hist[hex(a)].get(key,0)+1
        elif event["event"]=="vehicle_return": current["after"]=bytes(memory)
    def transaction(record,memory):
        _,pc,address,value,flags=record
        if flags&0x200 or not current or current["after"] is not None:return
        if pc==0x5c50a4:current["overflow"]=True
        if flags&0x100:
            for i in range(flags&255):
                if 0x20b100<=address+i<0x20b180:current["queue"].append((address+i,(value>>(8*i))&255))
    for step in replay_steps(groups,summary,paths["full-step-seed.bin"],paths["full-step-access.bin"],on_boundary=boundary,on_transaction=transaction):
        if current.get("after") is None or current["overflow"]:raise ValueError("Incomplete callback or original sound queue overflow")
        event=current["event"];obj,fp,pose=identity
        addresses=set()
        for a,n in ((obj,64),(fp,0x1f4//4),(pose,22),*SPANS):addresses.update(range(a,a+4*n,4))
        spans=ranges(addresses)
        stream.write(struct.pack("<5I",step["frame"],fp,obj,pose,current["queue_cursor"]))
        stream.write(struct.pack("<12I",*current["inputs"]))
        stream.write(struct.pack("<3I",*current["reference"]))
        stream.write(struct.pack("<I",len(spans)))
        for a,n in spans:stream.write(struct.pack(f"<{n+2}I",a,n,*words(current["after"],a,n)))
        stream.write(struct.pack("<I",len(current["queue"])))
        for a,v in current["queue"]:stream.write(struct.pack("<2I",a,v))
        rows.append({"frame":step["frame"],"ordinary_expected_words":len(addresses),"queue_bytes":len(current["queue"]),
                     "start_instructions":event["instructions"],"end_instructions":step["boundaries"][-1]["instructions"]})
    return {"records":rows,"input_histograms":hist,"setup_words":invariant,"identity":identity}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--capture",type=Path,required=True);p.add_argument("--output",type=Path,required=True)
    args=p.parse_args();output=private_output(args.output)
    if output.exists():p.error("Output must be a new private directory")
    groups,summary,paths,hashes=read_full_step_capture(args.capture);source_hashes={s:digest(ROOT/s) for s in SOURCES}
    output.mkdir(parents=True);report={"schema":1,"status":"failed","format":"VEHSTEP1","capture_sha256":hashes,"sources_sha256":source_hashes,
        "scope":"Single initial ordinary-memory seed, explicit surrounding-game/cabinet inputs, expected-after canonical outputs; no intermediate reseeding",
        "input_addresses":INPUTS,"excluded_from_canonical_equality":["original stack temporaries and pointers","CPU/interrupt/device state","sound queue serialization and cursor ownership"]}
    try:
        with (output/"vehicle.bin.tmp").open("xb") as stream:report.update(extract(groups,summary,paths,stream))
        if source_hashes!={s:digest(ROOT/s) for s in SOURCES} or any(digest(Path(n))!=h for n,h in hashes.items()):raise ValueError("Extraction inputs changed")
        (output/"vehicle.bin.tmp").rename(output/"vehicle.bin")
        report.update(status="passed",record_count=len(groups),records_sha256=digest(output/"vehicle.bin"));print(json.dumps({k:report[k] for k in ("status","record_count","records_sha256")}))
    except Exception as error:report["error"]=str(error);raise
    finally:(output/"result.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
if __name__=="__main__":main()
