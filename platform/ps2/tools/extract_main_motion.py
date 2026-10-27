#!/usr/bin/env python3
"""Extract bounded post-balance inputs/results from one continuous RAM replay."""
import argparse
import json
from pathlib import Path
import struct

from check_contact_neighborhood import ROOT, digest, private_output
from full_step_capture import SOURCES as READER_SOURCES, blocks, offset, read_full_step_capture, replay_steps

SOURCES=["platform/ps2/tools/extract_main_motion.py",*READER_SOURCES]
END_PC=0x5d61e0
REQUIRED=((0x214270,26),(0x213964,2),(0x213870,1),(0x21396c,1),(0x2138c4,1),
          (0x2197b0,1),(0x20b1a4,3),(0x20b180,4),(0x213960,1),(0x214208,1),
          (0x214360,3),(0x214354,1),(0x20a560,1),(0x2139f8,1),(0x202008,1),(0x202048,1))
ENTRY_WORDS=(0x20a560,0x2020a8,0x2139d8,0x2142c0)


def words(memory,address,count):
    at=offset(address,4*count)
    if at is None or address&3 or not 1<=count<=4096:
        raise ValueError("Tail extraction requested unbounded ordinary RAM")
    return struct.unpack_from(f"<{count}I",memory,at)


def ranges(addresses):
    result=[]
    for address in sorted(addresses):
        if result and result[-1][0]+4*result[-1][1]==address: result[-1][1]+=1
        else: result.append([address,1])
    return result


def compact_record(frame,fp,obj,before,after,addresses,queue):
    spans=ranges(addresses)
    if len(spans)>1024 or len(queue)>4096: raise ValueError("Tail compact record exceeds bounded fields")
    result=bytearray(struct.pack("<4I",frame,fp,obj,len(spans)))
    for address,count in spans:
        result+=struct.pack("<2I",address,count)
        result+=struct.pack(f"<{2*count}I",*words(before,address,count),*words(after,address,count))
    result+=struct.pack("<I",len(queue))
    for address,value in queue:
        if not 0x20b100<=address<0x20b180 or not 0<=value<=255:
            raise ValueError("Tail queue output is outside its original byte ring")
        result+=struct.pack("<2I",address,value)
    return result


def extract(groups,summary,paths,stream):
    stream.write(b"MOTTAIL1"+struct.pack("<I",len(groups)))
    current={};entry=[];results=[];external_relevant=set();read_union=set();patch_union=set()
    scene_calls=0;last_scene_instruction=-1
    def boundary(event,memory):
        if event["event"]=="vehicle_entry":
            item={"frame":event["frame"],"object":event["object"],"controls":event["controls"],
                  "words":{hex(a):words(memory,a,1)[0] for a in ENTRY_WORDS},
                  "object_index_word":words(memory,event["object"]+0x88,1)[0],
                  "object_8c":words(memory,event["object"]+0x8c,1)[0]}
            entry.append(item)
        if event["event"]=="balance_helper_post":
            current.clear();current.update(event=event,before=bytes(memory),after=None,addresses=set(),queue=[],
                                           overflow=[],last_overflow=-1,end_instruction=None)
            spans=[(event["object"],64),(event["vehicle_frame_pointer"],0x1f4//4),*REQUIRED]
            spans += [(a,len(v)) for (name,a),v in blocks(event).items()]
            for a,n in spans: current["addresses"].update(range(a,a+4*n,4))
    def transaction(record,memory):
        nonlocal scene_calls,last_scene_instruction
        instruction,pc,address,value,flags=record
        if pc==0x5d2e90 and not flags&0x300 and instruction!=last_scene_instruction:
            scene_calls+=1;last_scene_instruction=instruction
        if not current or current["after"] is not None: return
        if flags&0x200: raise ValueError("Tail did not reach its original final boundary before the next callback")
        if pc==END_PC:
            current["after"]=bytes(memory);current["end_instruction"]=instruction;return
        width=flags&255
        if offset(address,width) is not None and address<0x5a0000:
            current["addresses"].update(range(address&~3,(address+width+3)&~3,4))
        if flags&0x100:
            for byte in range(width):
                a=address+byte
                if 0x20b100<=a<0x20b180: current["queue"].append((a,(value>>(8*byte))&255))
        # This original enqueue branch is reached only when pending+3>128.
        # Observe its executed guard; never infer overflow from output volume.
        if pc==0x5c50a4 and instruction!=current["last_overflow"]:
            current["overflow"].append({"instructions":instruction,"pending":words(memory,0x20b180,1)[0]})
            current["last_overflow"]=instruction
    for step in replay_steps(groups,summary,paths["full-step-seed.bin"],paths["full-step-access.bin"],
                             on_boundary=boundary,on_transaction=transaction):
        if not current or current["after"] is None or current["event"]["frame"]!=step["frame"]:
            raise ValueError("Tail capture lacks a uniquely ordered helper-return/final boundary")
        if current["overflow"]: raise ValueError("Original sound queue overflow occurred in extracted tail")
        event=current["event"]
        encoded=compact_record(step["frame"],event["vehicle_frame_pointer"],event["object"],
                               current["before"],current["after"],current["addresses"],current["queue"])
        stream.write(encoded)
        read_union.update(step["read_before_write"]);patch_union.update(step["external_writes"])
        external_relevant.update(set(step["read_before_write"])&set(step["external_writes"]))
        results.append({"frame":step["frame"],"start_instructions":event["instructions"],
                        "end_instructions":current["end_instruction"],"block_words":len(current["addresses"]),
                        "queue_bytes":len(current["queue"]),"original_queue_overflows":0,"record_bytes":len(encoded)})
    return {"records":results,"entry_coverage":entry,"scene_helper_33e90_calls":scene_calls,
            "same_step_external_input_addresses":sorted(external_relevant),
            "external_writes_intersecting_any_step_input":sorted(read_union&patch_union),
            "external_patch_note":"Other CPU code writes; intersected with actual read-before-write dependencies. Not all entries are cabinet controls."}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();output=private_output(args.output)
    if output.exists(): parser.error("tail output must be a new private directory")
    groups,summary,paths,hashes=read_full_step_capture(args.capture)
    source_hashes={s:digest(ROOT/s) for s in SOURCES}
    output.mkdir(parents=True)
    report={"schema":1,"status":"failed","format":"MOTTAIL1","sources_sha256":source_hashes,
            "capture_sha256":hashes,"scope":"Observed post-balance36760..before371e0; isolated fixture inputs, not consecutive native-state proof"}
    temporary=output/"motion.bin.tmp"
    try:
        with temporary.open("xb") as stream: report.update(extract(groups,summary,paths,stream))
        if source_hashes!={s:digest(ROOT/s) for s in SOURCES} or any(digest(Path(p))!=h for p,h in hashes.items()):
            raise ValueError("Tail sources or sealed capture changed during extraction")
        temporary.rename(output/"motion.bin")
        report.update(status="passed",records_sha256=digest(output/"motion.bin"),record_count=len(groups))
        print(json.dumps({"status":"passed","records":len(groups),"sha256":report["records_sha256"]}))
    except Exception as error:
        report["error"]=str(error);raise
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")


if __name__=="__main__": main()
