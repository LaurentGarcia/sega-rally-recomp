"""Validate bounded private main-callback records without importing game data."""
import json
from pathlib import Path

from check_contact_neighborhood import digest, private_output
from check_driving_contact import block, integer, saved_return

SOURCES = ["platform/ps2/tools/main_vehicle_capture.py",
           "platform/ps2/tools/check_driving_contact.py",
           "platform/ps2/tools/check_contact_neighborhood.py"]
KINDS = ["vehicle_entry", "prefix_pre", "prefix_post"] + ["slot_pre", "slot_post"]*4 + [
    "balance_helper_pre", "selection_pre", "selection_post", "balance_pre", "balance_post",
    "balance_helper_post", "vehicle_return"]
PCS = {"vehicle_entry": 0x5d3f40, "prefix_pre": 0x5d3ff0, "prefix_post": 0x5d4218,
       "slot_pre": 0x5d6200, "balance_helper_pre": 0x5d6870, "selection_pre": 0x5d689c,
       "selection_post": 0x5d68f4, "balance_pre": 0x5d6900, "balance_post": 0x5d69d0,
       "balance_helper_post": 0x5d5760, "vehicle_return": 0x5c9bac}
SLOT_RETURNS = (0x5d4b88, 0x5d4bb4, 0x5d4bf0, 0x5d4c28)


def ordinary(address, size):
    return not address & 3 and ((0x200000 <= address <= 0x240000-size) or
                               (0x500000 <= address <= 0x600000-size))


def _proof(event, expected):
    if not isinstance(event.get("return_frame"), dict) or saved_return(event) != expected:
        raise ValueError("Vehicle saved caller proof differs from the inspected caller")


def vehicle_envelopes(events, marker):
    """Return ordered complete parent groups after structural/provenance checks.

    This validates observed call/operand identities, not the numerical semantics
    of the full callback or any native replacement. Each group has eighteen
    records, with four distinct slot pairs in original call order.
    """
    if not isinstance(events, list) or not events or len(events)>1024 or \
       not isinstance(marker, dict) or marker.get("schema")!=1 or marker.get("complete") is not True:
        raise ValueError("Vehicle capture needs a bounded complete record sequence")
    first, last = integer(marker.get("first_frame"), 32), integer(marker.get("last_frame"), 32)
    if not 0<=first<=last<=10000 or last-first>1200:
        raise ValueError("Vehicle capture frame window is invalid")
    groups, group, seen_frames = [], [], set()
    previous_instruction, previous_frame, previous_call = -1, -1, 0
    entry = child = balance = None
    for event in events:
        if not isinstance(event, dict) or event.get("schema")!=1:
            raise ValueError("Vehicle event has invalid schema")
        kind = event.get("event")
        if kind!=KINDS[len(group)]:
            raise ValueError("Vehicle event order is incomplete, duplicated or unknown")
        call, obj = integer(event.get("call_id"), 64), integer(event.get("object"), 32)
        frame, instruction = integer(event.get("frame"), 64), integer(event.get("instructions"), 64)
        fp, pc = integer(event.get("vehicle_frame_pointer"), 32), integer(event.get("pc"), 32)
        regs, words, floats = event.get("registers"), event.get("object_words"), event.get("fp_bits")
        if not isinstance(regs,list) or len(regs)!=32 or not isinstance(words,list) or len(words)!=64 or \
           not isinstance(floats,list) or len(floats)!=4:
            raise ValueError("Vehicle event lacks complete raw register/object state")
        for value in regs+words: integer(value,32)
        for value in floats: integer(value,64)
        integer(event.get("cycles"),64);integer(event.get("board_ns"),64)
        if integer(event.get("ac"),32)>>30:
            raise ValueError("Vehicle numerical evidence requires nearest-even rounding")
        if not call or not ordinary(obj,0x100) or not ordinary(fp,0x1f4) or fp&63 or \
           instruction<=previous_instruction or frame<previous_frame or not first<=frame<=last+1:
            raise ValueError("Vehicle event identity, RAM range or ordering is invalid")
        previous_instruction,previous_frame=instruction,frame
        blocks=event.get("blocks")
        if not isinstance(blocks,list) or len(blocks)>16 or any(not isinstance(b,dict) or not isinstance(b.get("name"),str) for b in blocks) or \
           len({b.get("name") for b in blocks})!=len(blocks):
            raise ValueError("Vehicle RAM blocks are malformed or duplicated")
        for data in blocks:
            address=integer(data.get("address"),32)
            values=data.get("words")
            if not isinstance(values,list) or not 1<=len(values)<=64 or not ordinary(address,4*len(values)):
                raise ValueError("Vehicle block is outside bounded ordinary RAM")
            for value in values: integer(value,32)
        if block(event,"main_object_pointer",0x2140cc,1)!=[obj] or words[1]!=0x5d3f40:
            raise ValueError("Vehicle callback is not bound to its published object pointer")
        if kind!="slot_post" and pc!=PCS[kind]:
            raise ValueError("Vehicle event has the wrong inspected instruction boundary")
        if kind=="vehicle_entry":
            if call<=previous_call or frame in seen_frames or frame>last or regs[16]!=obj or regs[31]!=fp:
                raise ValueError("Vehicle parent entry is repeated or mismatched")
            _proof(event,(0x5c9bac,regs[0]&~63))
            if event.get("parent_return_frame")!=event["return_frame"]:
                raise ValueError("Vehicle parent proof is inconsistent")
            entry=event;previous_call=call;seen_frames.add(frame)
        elif call!=entry["call_id"] or obj!=entry["object"] or fp!=entry["vehicle_frame_pointer"] or \
             event.get("parent_return_frame")!=entry["return_frame"] or \
             not 1<=instruction-entry["instructions"]<=2000000:
            raise ValueError("Vehicle event does not belong to its bounded unchanged parent")
        if kind.startswith("prefix_"):
            if regs[31]!=fp or regs[28 if kind=="prefix_pre" else 16]!=obj or event.get("return_frame")!=entry["return_frame"]:
                raise ValueError("Vehicle prefix has changed frame, object or parent")
            _proof(event,(entry["return_frame"]["pc"],entry["return_frame"]["frame_pointer"]))
            block(event,"prefix_global",0x213974,1)
            block(event,"candidate_position",fp+0x130,3)
            block(event,"candidate_orientation",fp+0x140,3)
            if kind=="prefix_post" and (regs[17]!=fp+0x130 or regs[18]!=fp+0x140):
                raise ValueError("Vehicle candidate pointers differ from contact-call arguments")
        if kind.startswith("slot_"):
            index=(len(group)-3)//2
            helper=integer(event.get("helper_frame_pointer"),32)
            if integer(event.get("slot_index"),32)!=index or not ordinary(helper,0x80) or helper&63:
                raise ValueError("Vehicle slot index or helper frame is invalid")
            if kind=="slot_pre":
                if regs[31]!=helper or regs[16]!=obj or regs[22]!=index or regs[30] or regs[20]!=0x3e800000 or \
                   regs[17]!=fp+0x40+12*index or regs[18]!=fp+0xf0+12*index or \
                   regs[19]!=fp+0xa0+12*(index//2) or regs[21]!=fp+0x70+12*index:
                    raise ValueError("Vehicle slot operands differ from the indexed parent arrays")
                _proof(event,(SLOT_RETURNS[index],fp));child=event
            elif pc!=SLOT_RETURNS[index] or regs[31]!=fp or helper!=child["helper_frame_pointer"] or \
                 event.get("return_frame")!=child["return_frame"]:
                raise ValueError("Vehicle slot result does not restore its saved caller")
            if kind=="slot_post":
                _proof(dict(event,return_frame=entry["return_frame"]),
                       (entry["return_frame"]["pc"],entry["return_frame"]["frame_pointer"]))
            for name,reg in (("slot_vector",17),("slot_triple",18),("slot_direction",19),("slot_output",21)):
                block(event,name,child["registers"][reg],3)
            block(event,"slot_result",0x2138d0+36*index,9)
            block(event,"slot_globals",0x213880,16)
            block(event,"slot_mask",0x214208,1)
            block(event,"slot_parameters",0x2141d0,5)
            block(event,"slot_control",0x214124,1)
        if kind.startswith("balance_") or kind.startswith("selection_"):
            helper=integer(event.get("helper_frame_pointer"),32)
            if not ordinary(helper,0x80) or helper&63:
                raise ValueError("Vehicle balance helper frame is outside ordinary RAM")
            if kind=="balance_helper_pre":
                if regs[31]!=helper or regs[16]!=obj or regs[17]!=fp+0x120 or regs[18]!=fp+0x150:
                    raise ValueError("Vehicle balance pointers differ from caller workspace")
                _proof(event,(0x5d5760,fp));balance=event
            elif helper!=balance["helper_frame_pointer"] or event.get("return_frame")!=balance["return_frame"]:
                raise ValueError("Vehicle balance helper changed its saved identity")
            elif kind=="balance_helper_post":
                if regs[31]!=fp:
                    raise ValueError("Vehicle balance result did not restore caller frame")
                _proof(dict(event,return_frame=entry["return_frame"]),
                       (entry["return_frame"]["pc"],entry["return_frame"]["frame_pointer"]))
            else:
                if regs[31]!=helper or regs[9]!=obj or regs[3]!=balance["registers"][17] or regs[15]!=balance["registers"][18]:
                    raise ValueError("Vehicle scalar boundary differs from saved helper arguments")
                _proof(event,(0x5d5760,fp))
            if kind.startswith("balance_"):
                block(event,"balance_vector",balance["registers"][17],3)
                block(event,"balance_output",balance["registers"][18],3)
                block(event,"balance_pairs",obj+0x64,4)
                block(event,"balance_prior",0x21425c,1)
            else:
                block(event,"selection_flags",0x214208,1)
                block(event,"selected_parameters",0x2141c0,13)
        group.append(event)
        if kind=="vehicle_return":
            if regs[31]!=entry["return_frame"]["frame_pointer"] or event.get("return_frame")!=entry["return_frame"]:
                raise ValueError("Vehicle return does not restore the saved dispatcher frame")
            groups.append(group);group=[]
    calls=integer(marker.get("vehicle_calls"),64)
    samples=integer(marker.get("vehicle_samples"),64)
    if group or not 0<len(groups)==samples<=min(calls,32) or calls>16384 or previous_call>calls or \
       integer(marker.get("vehicle_events"),64)!=len(events) or integer(marker.get("vehicle_completions"),64)!=calls:
        raise ValueError("Vehicle completion/sample counts are incomplete or inconsistent")
    for stem,multiplier in (("prefix",1),("slot",4),("balance",1),("selection",1)):
        if any(integer(marker.get("vehicle_"+stem+suffix),64)!=multiplier*count
               for suffix,count in (("_entries",calls),("_completions",calls),("_samples",samples))):
            raise ValueError("Vehicle child completion/sample counts differ: "+stem)
    if any(integer(marker.get(key),64)!=calls for key in ("vehicle_balance_calls","vehicle_balance_returns")):
        raise ValueError("Vehicle balance helper completion counts differ")
    return groups


def read_vehicle_capture(directory):
    """Bind a completed private stream to the sealed observer parity report."""
    directory=private_output(Path(directory))
    paths=[directory/"capture.json",directory/"main-vehicle.jsonl",directory.parent/"validation.json"]
    for path,bound in zip(paths,(32768,16*1024*1024,1024*1024)):
        private_output(path)
        if path.is_symlink() or not path.is_file() or path.stat().st_size>bound:
            raise ValueError("Vehicle capture file is missing, redirected or exceeds its bound")
    hashes={str(path):digest(path) for path in paths}
    marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or \
       validation.get("status")!="passed" or validation.get("exact_checkpoint_match") is not True or \
       validation.get("capture_summary")!=marker or \
       integer(validation.get("frames"),32)!=integer(validation.get("matched_frames"),32) or \
       not 0<validation["frames"]<=10000 or validation.get("baseline_sha256")!=validation.get("captured_sha256"):
        raise ValueError("Vehicle capture lacks complete, count-matched observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:
            raise ValueError("Vehicle capture differs from its validated identity")
    for name in ("baseline","captured"):
        path=directory.parent/(name+".csv")
        private_output(path)
        if path.is_symlink() or not path.is_file() or path.stat().st_size>4*1024*1024:
            raise ValueError("Vehicle parity checkpoint file is unavailable or oversized")
        hashes[str(path)]=digest(path)
        if hashes[str(path)]!=validation[name+"_sha256"]:
            raise ValueError("Vehicle checkpoint identity differs from its parity report")
    events=[json.loads(line) for line in paths[1].read_text().splitlines()]
    vehicle_envelopes(events,marker)
    if any(digest(Path(path))!=value for path,value in hashes.items()):
        raise ValueError("Vehicle capture inputs changed during validation")
    return events,marker,hashes
