"""Validate and replay actual CPU data dependencies of consecutive player calls.

This is an offline observation interface, not a CPU interpreter or a native
vehicle implementation. It proves ordinary-memory continuity for the observed
execution. Device responses remain recorded transactions, not a seeded device.
"""
import argparse
import json
from pathlib import Path
import struct

from check_contact_neighborhood import digest, private_output
from check_driving_contact import integer
from main_vehicle_capture import SOURCES as VEHICLE_SOURCES, read_vehicle_capture, vehicle_envelopes

SOURCES = ["platform/ps2/tools/full_step_capture.py", *VEHICLE_SOURCES]
REGIONS = ((0x200000,0x40000),(0x500000,0x100000))
GLOBALS = ((0x202008,1),(0x202018,1),(0x202044,4),(0x202098,1),(0x2020b4,1),(0x20b0b4,1),
           (0x2139cc,2),(0x214120,2),(0x2141c0,13),(0x214200,2),(0x214208,1),(0x21420c,4),
           (0x214230,10),(0x214258,4),(0x214270,26),(0x2142e0,21),(0x2197b0,1),(0x2140cc,1))
KINDS = ("vehicle_entry","balance_helper_pre","balance_helper_post","vehicle_return")
PCS = (0x5d3f40,0x5d6870,0x5d5760,0x5c9bac)
RECORD = struct.Struct("<QIIII")
LIMIT = 256*1024*1024


def offset(address, width):
    base=0
    for start,size in REGIONS:
        if start<=address and address+width<=start+size:
            return base+address-start
        base+=size
    return None


def blocks(event):
    result={}
    for data in event.get("blocks",[]):
        key=(data.get("name"),integer(data.get("address"),32))
        values=data.get("words")
        if key in result or not isinstance(values,list) or not 1<=len(values)<=64 or \
           key[1]&3 or offset(key[1],4*len(values)) is None:
            raise ValueError("Full-step block is duplicated or outside ordinary RAM")
        for value in values: integer(value,32)
        result[key]=values
    return result


def proof(value):
    if not isinstance(value,dict) or set(value)!={"pc","frame_pointer","words"}:
        raise ValueError("Full-step return proof is incomplete")
    pc,fp=integer(value["pc"],32),integer(value["frame_pointer"],32)
    words=value["words"]
    if not pc or pc&3 or fp&63 or offset(fp,64) is None or not isinstance(words,list) or len(words)!=16:
        raise ValueError("Full-step saved frame is malformed")
    for word in words: integer(word,32)
    if words[2]!=pc: raise ValueError("Full-step saved RIP disagrees with frame")
    return pc,fp


def step_envelopes(events,summary):
    if not isinstance(summary,dict) or summary.get("schema")!=1 or summary.get("complete") is not True or \
       summary.get("record_bytes")!=RECORD.size or summary.get("seed_regions")!=[list(r) for r in REGIONS]:
        raise ValueError("Full-step summary is incomplete or has incompatible memory layout")
    count=integer(summary.get("steps"),32)
    first,last=integer(summary.get("first_frame"),32),integer(summary.get("last_frame"),32)
    total=integer(summary.get("transactions"),64)
    if not 1<=count<=1201 or last-first+1!=count or last>10000 or len(events)!=count*4 or total>LIMIT//24:
        raise ValueError("Full-step capture is not bounded and consecutive")
    previous_instruction=-1;previous_index=0;result=[]
    for step in range(count):
        group=events[4*step:4*step+4];entry=group[0];helper=group[1]
        obj=integer(entry.get("object"),32);fp=integer(entry.get("vehicle_frame_pointer"),32)
        if offset(obj,256) is None or obj&3 or fp&63 or offset(fp,0x1f4) is None:
            raise ValueError("Full-step parent storage is invalid")
        if step and (obj!=events[0]["object"] or entry["call_id"]!=events[4*(step-1)]["call_id"]+1):
            raise ValueError("Full-step callback sequence changes object or skips a call")
        for stage,event in enumerate(group):
            if not isinstance(event,dict) or event.get("schema")!=1 or event.get("event")!=KINDS[stage] or event.get("pc")!=PCS[stage]:
                raise ValueError("Full-step boundary is missing, duplicated or misplaced")
            regs=event.get("registers");floats=event.get("fp_bits");controls=event.get("controls")
            if not isinstance(regs,list) or len(regs)!=32 or not isinstance(floats,list) or len(floats)!=4 or \
               not isinstance(controls,list) or len(controls)!=12:
                raise ValueError("Full-step machine state or cabinet pins are incomplete")
            for value in regs: integer(value,32)
            for value in floats: integer(value,64)
            for value in controls: integer(value,8)
            instruction=integer(event.get("instructions"),64);index=integer(event.get("transaction_index"),64)
            integer(event.get("cycles"),64);integer(event.get("board_ns"),64);integer(event.get("processor_control"),32)
            if integer(event.get("ac"),32)>>30 or instruction<=previous_instruction or not previous_index<=index<=total:
                raise ValueError("Full-step order or rounding control differs")
            previous_instruction,previous_index=instruction,index
            if event.get("frame")!=first+step or event.get("controls_frame")!=first+step or \
               event.get("controls")!=entry["controls"] or event.get("object")!=obj or \
               event.get("vehicle_frame_pointer")!=fp or event.get("call_id")!=entry["call_id"] or \
               event.get("parent_return")!=entry.get("parent_return"):
                raise ValueError("Full-step parent or controls changed within one callback")
            parent_pc,parent_fp=proof(event.get("parent_return"))
            if parent_pc!=0x5c9bac or (stage==0 and (regs[0]&~63!=parent_fp or regs[16]!=obj or regs[31]!=fp)):
                raise ValueError("Full-step caller identity differs")
            data=blocks(event)
            expected={("object",obj),*(("global",a) for a,n in GLOBALS)}
            if stage:
                expected|={("balance_vector",helper["registers"][17]),("balance_output",helper["registers"][18])}
                helper_pc,helper_parent=proof(event.get("helper_return"))
                if helper_pc!=0x5d5760 or helper_parent!=fp or event.get("helper_return")!=helper.get("helper_return") or \
                   event.get("helper_frame_pointer")!=helper.get("helper_frame_pointer"):
                    raise ValueError("Full-step helper proof changed")
                helper_fp=integer(event.get("helper_frame_pointer"),32)
                if helper_fp&63 or offset(helper_fp,0x80) is None or \
                   regs[31]!=(helper_fp if stage==1 else fp if stage==2 else parent_fp):
                    raise ValueError("Full-step helper failed to restore its caller")
                if stage==1 and (regs[16]!=obj or regs[0]&~63!=fp):
                    raise ValueError("Full-step helper arguments differ")
                for name,reg in (("balance_vector",17),("balance_output",18)):
                    if len(data.get((name,helper["registers"][reg]),[]))!=3:
                        raise ValueError("Full-step helper vector is not three words")
            if set(data)!=expected or len(data[("object",obj)])!=64 or data[("global",0x2140cc)]!=[obj] or \
               data[("object",obj)][1]!=0x5d3f40 or any(len(data[("global",a)])!=n for a,n in GLOBALS):
                raise ValueError("Full-step complete state blocks differ from the declared closure")
        if group[-1]["instructions"]-entry["instructions"]>2000000:
            raise ValueError("Full-step instruction extent exceeds bound")
        result.append(group)
    if previous_index!=total: raise ValueError("Full-step trace has transactions after its final return")
    return result


def replay_steps(groups,summary,seed_path,access_path,*,on_boundary=None,on_transaction=None):
    """Yield actual per-step first-read bytes, final written bytes and external inputs.

    Feed one initial seed into a future native comparison; apply only the
    separately reported external patches at each next step. Never substitute
    the expected writes for native state. All reads and boundary snapshots are
    checked against this continuously evolved ordinary-memory shadow. Optional
    callbacks receive (boundary/transaction, readonly_memory_view) before each
    transaction or after a validated boundary. Copy bytes that must outlive the
    callback: the backing shadow continues to evolve. This supplies exact RAM
    to isolated original-function or native comparisons without new snapshots.
    """
    seed_path,access_path=Path(seed_path),Path(access_path)
    if seed_path.stat().st_size!=0x140000 or access_path.stat().st_size!=8+summary["transactions"]*24:
        raise ValueError("Full-step binary length differs from its complete marker")
    memory=bytearray(seed_path.read_bytes());index=0
    with access_path.open("rb") as stream:
        if stream.read(8)!=b"RLYSTEP1": raise ValueError("Full-step transaction header differs")
        for group in groups:
            reads={};writes={};external={};devices=[];immutable=[];last_instruction=group[0]["instructions"]
            for stage,boundary in enumerate(group):
                while index<boundary["transaction_index"]:
                    record=stream.read(24)
                    if len(record)!=24: raise ValueError("Truncated full-step transaction")
                    instruction,pc,address,value,flags=RECORD.unpack(record);index+=1
                    width=flags&255;writing=bool(flags&0x100);outside=bool(flags&0x200)
                    if width not in (1,2,4) or flags&~0x3ff or address+width>2**32 or value>=2**(8*width) or pc&3:
                        raise ValueError("Malformed full-step transaction")
                    at=offset(address,width)
                    if stage==0:
                        if not outside or not writing or at is None or instruction>=boundary["instructions"]:
                            raise ValueError("Between-step transaction is not an external ordinary write")
                    elif outside or not last_instruction<=instruction<boundary["instructions"]:
                        raise ValueError("Full-step transaction is outside the ordered callback")
                    else: last_instruction=instruction
                    raw=value.to_bytes(width,"little")
                    if on_transaction is not None:
                        on_transaction((instruction,pc,address,value,flags),memoryview(memory).toreadonly())
                    if at is None:
                        item=(instruction,pc,address,value,flags)
                        if (address<0x200000 or 0x2000000<=address<0x4000000 or 0x6000000<=address<0x7000000): immutable.append(item)
                        else: devices.append(item)
                    elif writing:
                        memory[at:at+width]=raw
                        target=external if outside else writes
                        for byte,v in enumerate(raw): target[address+byte]=v
                    else:
                        if memory[at:at+width]!=raw:
                            raise ValueError(f"Unaccounted ordinary-memory mutation at {address:#x}, instruction {instruction}")
                        for byte,v in enumerate(raw):
                            a=address+byte
                            if a not in writes: reads.setdefault(a,v)
                for (name,address),values in blocks(boundary).items():
                    expected=b"".join(v.to_bytes(4,"little") for v in values)
                    at=offset(address,len(expected))
                    if memory[at:at+len(expected)]!=expected:
                        raise ValueError(f"Untraced memory change in {boundary['event']} {name} at {address:#x}")
                if on_boundary is not None: on_boundary(boundary,memoryview(memory).toreadonly())
            yield {"call_id":group[0]["call_id"],"frame":group[0]["frame"],"boundaries":group,
                   "read_before_write":reads,"final_writes":writes,"external_writes":external,
                   "device_transactions":devices,"immutable_transactions":immutable}
        if stream.read(1): raise ValueError("Trailing full-step transaction data")


def read_full_step_capture(directory):
    directory=private_output(Path(directory))
    vehicle,marker,hashes=read_vehicle_capture(directory)
    validation=json.loads((directory.parent/"validation.json").read_text())
    paths={name:directory/name for name in ("full-step.jsonl","full-step-summary.json","full-step-seed.bin","full-step-access.bin")}
    for name,path in paths.items():
        if path.is_symlink() or not path.is_file() or path.stat().st_size>LIMIT:
            raise ValueError("Full-step evidence is missing, redirected or oversized")
        value=digest(path);hashes[str(path)]=value
        if validation.get("capture_files_sha256",{}).get(name)!=value:
            raise ValueError("Full-step evidence differs from the parity-validated capture")
    summary=json.loads(paths["full-step-summary.json"].read_text())
    events=[json.loads(line) for line in paths["full-step.jsonl"].read_text().splitlines()]
    groups=step_envelopes(events,summary)
    if summary["steps"]!=marker["vehicle_completions"] or summary["first_frame"]!=marker["first_frame"] or summary["last_frame"]!=marker["last_frame"]:
        raise ValueError("Full-step and sampled callback completion markers differ")
    for sampled in vehicle_envelopes(vehicle,marker):
        whole=groups[sampled[0]["frame"]-summary["first_frame"]]
        for observed,old in zip(whole,(sampled[0],sampled[11],sampled[16],sampled[17])):
            if any(observed[key]!=old[key] for key in ("pc","frame","call_id","instructions","ac","registers","fp_bits","object")) or \
               blocks(observed)[("object",observed["object"])]!=old["object_words"] or \
               observed["parent_return"]!={key:old["parent_return_frame"][key] for key in ("pc","frame_pointer","words")}:
                raise ValueError("Full-step capture disagrees with the existing sampled observer")
    if any(digest(Path(path))!=expected for path,expected in hashes.items()): raise ValueError("Evidence changed during reading")
    return groups,summary,paths,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();output=private_output(args.output)
    if output.exists(): parser.error("output report must not already exist")
    groups,summary,paths,hashes=read_full_step_capture(args.capture)
    reads=set();writes=set();external=set();device_addresses=set();immutable_addresses=set();steps=[]
    for step in replay_steps(groups,summary,paths["full-step-seed.bin"],paths["full-step-access.bin"]):
        reads.update(step["read_before_write"]);writes.update(step["final_writes"]);external.update(step["external_writes"])
        device_addresses.update(t[2] for t in step["device_transactions"]);immutable_addresses.update(t[2] for t in step["immutable_transactions"])
        steps.append({"frame":step["frame"],"read_before_write_bytes":len(step["read_before_write"]),
                      "written_bytes":len(step["final_writes"]),"external_patch_bytes":len(step["external_writes"]),
                      "device_transactions":len(step["device_transactions"]),"immutable_transactions":len(step["immutable_transactions"])})
    def spans(values):
        result=[]
        for a in sorted(values):
            if result and result[-1][0]+result[-1][1]==a: result[-1][1]+=1
            else: result.append([a,1])
        return result
    root=Path(__file__).resolve().parents[3]
    report={"schema":1,"status":"passed","scope":"Continuous observed ordinary-memory replay; no native tick compared and no device-state completeness claim",
            "capture_files_sha256":hashes,"sources_sha256":{s:digest(root/s) for s in SOURCES},"summary":summary,"steps":steps,
            "read_before_write_spans":spans(reads),"written_spans":spans(writes),"external_patch_spans":spans(external),
            "device_addresses":sorted(device_addresses),"immutable_addresses":sorted(immutable_addresses)}
    if any(digest(Path(path))!=expected for path,expected in hashes.items()): raise ValueError("Evidence changed during replay")
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("x") as stream: json.dump(report,stream,indent=2,sort_keys=True);stream.write("\n")
    print(json.dumps({"status":"passed","steps":len(steps),"ordinary_input_bytes":len(reads),"ordinary_written_bytes":len(writes)}))


if __name__=="__main__": main()
