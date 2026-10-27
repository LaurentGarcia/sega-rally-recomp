"""Bind additive motion observations to the unchanged main-vehicle envelopes."""
import json
from pathlib import Path

from check_contact_neighborhood import digest, private_output
from check_driving_contact import block, integer, saved_return
from main_vehicle_capture import SOURCES as VEHICLE_SOURCES, ordinary, read_vehicle_capture, vehicle_envelopes

SOURCES = ["platform/ps2/tools/main_motion_capture.py", *VEHICLE_SOURCES]
KINDS = ("segment_pre", "segment_post", "motion_tail_pre", "motion_tail_post")
PCS = (0x5ca0b0, 0x5d3fc8, 0x5d72f8, 0x5d7658)


def motion_envelopes(events, vehicle_events, marker):
    """Return four-event groups after validating their existing parent witnesses.

    Raw numerical state is retained; this layer proves observed identities and
    bounds only. Each component's original/native comparison establishes its
    supported arithmetic domain separately.
    """
    parents=vehicle_envelopes(vehicle_events,marker)
    if not isinstance(events,list) or len(events)!=4*len(parents) or len(events)>128:
        raise ValueError("Motion capture requires four complete events per sampled parent")
    calls,samples=integer(marker.get("vehicle_calls"),64),len(parents)
    for kind in ("segment","tail"):
        if any(integer(marker.get("motion_"+kind+suffix),64)!=count
               for suffix,count in (("_entries",calls),("_completions",calls),("_samples",samples))):
            raise ValueError("Motion component completion counts differ")
    if integer(marker.get("motion_events"),64)!=len(events):
        raise ValueError("Motion stream event count differs")
    groups=[];last_instruction=-1
    for index,parent in enumerate(parents):
        group=events[4*index:4*index+4]
        entry,prefix=parent[:2];helper=parent[11];balanced=parent[15];returned=parent[16]
        obj,fp=entry["object"],entry["vehicle_frame_pointer"]
        segment_helper=None
        for stage,event in enumerate(group):
            if not isinstance(event,dict) or event.get("schema")!=1 or event.get("event")!=KINDS[stage] or event.get("pc")!=PCS[stage]:
                raise ValueError("Motion event has an unknown, misplaced or repeated boundary")
            registers,words,floats=event.get("registers"),event.get("object_words"),event.get("fp_bits")
            if not isinstance(registers,list) or len(registers)!=32 or not isinstance(words,list) or len(words)!=64 or \
               not isinstance(floats,list) or len(floats)!=4:
                raise ValueError("Motion event lacks bounded raw machine state")
            for value in registers+words: integer(value,32)
            for value in floats: integer(value,64)
            instruction=integer(event.get("instructions"),64)
            frame=integer(event.get("frame"),64)
            integer(event.get("cycles"),64);integer(event.get("board_ns"),64)
            if integer(event.get("ac"),32)>>30 or instruction<=last_instruction or \
               not entry["instructions"]<instruction<parent[-1]["instructions"] or \
               not entry["frame"]<=frame<=parent[-1]["frame"]:
                raise ValueError("Motion ordering, parent extent or rounding control differs")
            last_instruction=instruction
            if event.get("call_id")!=entry["call_id"] or event.get("object")!=obj or \
               event.get("vehicle_frame_pointer")!=fp or event.get("parent_return_frame")!=entry["return_frame"]:
                raise ValueError("Motion boundary does not belong to its unchanged parent")
            helper_fp=integer(event.get("helper_frame_pointer"),32)
            if helper_fp&63 or not ordinary(helper_fp,0x80):
                raise ValueError("Motion helper frame is outside ordinary RAM")
            blocks=event.get("blocks")
            if not isinstance(blocks,list) or len(blocks)!=4 or \
               any(not isinstance(data,dict) or not isinstance(data.get("name"),str) for data in blocks) or \
               len({data["name"] for data in blocks})!=4:
                raise ValueError("Motion RAM blocks are incomplete or duplicated")
            for data in blocks:
                address=integer(data.get("address"),32);values=data.get("words")
                if not isinstance(values,list) or not 1<=len(values)<=3 or not ordinary(address,4*len(values)):
                    raise ValueError("Motion block is outside bounded ordinary RAM")
                for value in values: integer(value,32)
            if block(event,"main_object_pointer",0x2140cc,1)!=[obj] or words[1]!=0x5d3f40:
                raise ValueError("Motion object differs from the published callback")
            if stage<2:
                if instruction>=prefix["instructions"]:
                    raise ValueError("Segment selection does not precede candidate preparation")
                if stage==0:
                    if registers[31]!=helper_fp or registers[16]!=obj+0x14 or registers[17]!=obj+0x88 or \
                       not isinstance(event.get("return_frame"),dict) or saved_return(event)!=(0x5d3fc8,fp):
                        raise ValueError("Segment entry operands or saved caller differ")
                    segment_helper=event
                else:
                    if helper_fp!=segment_helper["helper_frame_pointer"] or registers[31]!=fp or \
                       event.get("return_frame")!=segment_helper["return_frame"] or \
                       saved_return(dict(event,return_frame=entry["return_frame"]))!=saved_return(entry):
                        raise ValueError("Segment result did not restore the verified caller")
                if block(event,"segment_position",obj+0x14,3)!=words[5:8] or \
                   block(event,"segment_indices",obj+0x88,1)!=words[34:35]:
                    raise ValueError("Segment snapshots differ from their exact object storage")
                block(event,"course_selector",0x214354,1)
            else:
                if not balanced["instructions"]<instruction<returned["instructions"] or \
                   helper_fp!=helper["helper_frame_pointer"] or registers[31]!=helper_fp or \
                   event.get("return_frame")!=helper["return_frame"] or saved_return(event)!=(0x5d5760,fp):
                    raise ValueError("Motion tail lacks its verified enclosing helper")
                vector,output=helper["registers"][17:19]
                if stage==2 and (registers[9]!=obj or registers[3]!=vector or registers[15]!=output):
                    raise ValueError("Motion tail pointers differ from saved helper arguments")
                block(event,"balance_vector",vector,3);block(event,"balance_output",output,3)
                block(event,"motion_prior",0x214264,1)
        groups.append(group)
    return groups


def read_motion_capture(directory):
    """Return motion events, original vehicle events, marker and sealed hashes."""
    directory=private_output(Path(directory))
    vehicle_events,marker,hashes=read_vehicle_capture(directory)
    path=directory/"main-motion.jsonl";private_output(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size>1024*1024:
        raise ValueError("Motion stream is missing, redirected or oversized")
    hashes[str(path)]=digest(path)
    validation=json.loads((directory.parent/"validation.json").read_text())
    if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:
        raise ValueError("Motion stream differs from its parity-validated identity")
    events=[json.loads(line) for line in path.read_text().splitlines()]
    motion_envelopes(events,vehicle_events,marker)
    if any(digest(Path(name))!=expected for name,expected in hashes.items()):
        raise ValueError("Motion capture changed during validation")
    return events,vehicle_events,marker,hashes
