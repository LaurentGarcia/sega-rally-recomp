"""Strict private main-response normalization pairs; no original data embedded."""
import json
from pathlib import Path

from check_contact_neighborhood import digest, private_output
from check_contact_result import result_rows
from check_driving_contact import block, integer, saved_return

SOURCES = ["platform/ps2/tools/contact_normalization_live.py",
           "platform/ps2/tools/check_contact_result.py",
           "platform/ps2/tools/check_contact_query.py",
           "platform/ps2/tools/check_driving_contact.py",
           "platform/ps2/tools/check_contact_count.py",
           "platform/ps2/tools/check_contact_neighborhood.py"]


def normalization_rows(events, marker):
    """Return six raw words per pair, with complete parent/contact provenance."""
    # This binds every sampled pass, its four point/slot identities, actual
    # pipelined query/result order and restored caller frames before recovery.
    result_rows(events, marker, "main-contact")
    pending = None
    seen, rows, provenance = set(), [], []
    candidates = None
    for event in events:
        kind = event["event"]
        if kind == "main_entry":
            candidates = event["registers"][17:19]
        if kind not in ("normalization_pre", "normalization_post"):
            if pending is not None:
                raise ValueError("Unexpected main event inside normalization pair")
            continue
        regs = event["registers"]
        call, obj = event["call_id"], event["object"]
        fp, index = event["main_frame_pointer"], integer(event.get("contact_index"), 32)
        frame, instruction = event["frame"], event["instructions"]
        if index >= 4 or regs[5] != index or regs[7] != fp+0xd0 or regs[25] != fp+0xd8 or \
           regs[31] != fp or regs[13] != obj or regs[30] != 0 or integer(event.get("ac"), 32) >> 30:
            raise ValueError("Unsupported normalization frame, pointers, zero operand or rounding")
        if not isinstance(event.get("return_frame"), dict) or event.get("return_frame") != event.get("parent_return_frame"):
            raise ValueError("Normalization lacks unchanged saved parent proof")
        returning = saved_return(event)
        vector = block(event, "normalization_vector", fp+0xd0, 3)
        position = block(event, "candidate_position", _address(event, "candidate_position"), 3)
        orientation = block(event, "candidate_orientation", _address(event, "candidate_orientation"), 3)
        if candidates != [_address(event, "candidate_position"), _address(event, "candidate_orientation")]:
            raise ValueError("Normalization candidate blocks differ from parent entry arguments")
        key = (call, obj, index)
        if kind == "normalization_pre":
            if pending is not None or key in seen or len(rows) >= 128 or event["pc"] != 0x5d38ec:
                raise ValueError("Nested, repeated or excessive normalization input")
            if any((word & 0x7fffffff) > 0x49800000 for word in vector):
                raise ValueError("Normalization input exceeds native finite domain")
            pending = {"key": key, "fp": fp, "frame": frame, "instruction": instruction,
                       "returning": returning, "proof": event["return_frame"], "vector": vector,
                       "object_words": event["object_words"], "position": position, "orientation": orientation,
                       "position_address": _address(event, "candidate_position"),
                       "orientation_address": _address(event, "candidate_orientation")}
            continue
        if pending is None or key != pending["key"] or fp != pending["fp"] or frame < pending["frame"] or \
           not 1 <= instruction-pending["instruction"] <= 2000000 or event["pc"] != 0x5d3968 or \
           returning != pending["returning"] or event["return_frame"] != pending["proof"] or \
           event["object_words"] != pending["object_words"] or position != pending["position"] or \
           orientation != pending["orientation"] or _address(event, "candidate_position") != pending["position_address"] or \
           _address(event, "candidate_orientation") != pending["orientation_address"]:
            raise ValueError("Unpaired normalization result or changed unrelated state")
        rows.append(tuple(pending["vector"]+vector))
        provenance.append({"call_id": call, "object": obj, "contact_index": index,
                           "entry_frame": pending["frame"], "exit_frame": frame,
                           "instructions": instruction-pending["instruction"], "return_pc": returning[0]})
        seen.add(key)
        pending = None
    count = integer(marker.get("main_normalization_entries"), 64)
    if pending is not None or not rows or count != integer(marker.get("main_normalization_completions"), 64) or \
       len(rows) != integer(marker.get("main_normalization_sampled_pairs"), 64) or \
       not len(rows) <= count <= 4*integer(marker.get("main_calls"), 64):
        raise ValueError("Incomplete, empty or miscounted normalization capture")
    return rows, provenance


def _address(event, name):
    values = [entry for entry in event.get("blocks", []) if isinstance(entry, dict) and entry.get("name") == name]
    if len(values) != 1:
        raise ValueError("Missing or duplicated normalization state block: "+name)
    address = integer(values[0].get("address"), 32)
    if address & 3 or not ((0x200000 <= address <= 0x240000-12) or (0x500000 <= address <= 0x600000-12)):
        raise ValueError("Normalization candidate address outside ordinary RAM")
    return address


def read_capture(directory):
    directory = private_output(Path(directory))
    paths = [directory/"capture.json", directory/"main-contact.jsonl", directory.parent/"validation.json"]
    for path, limit in zip(paths, (16384, 32*1024*1024, 1024*1024)):
        if path.stat().st_size > limit:
            raise ValueError("Normalization capture exceeds reader bound")
    hashes = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("schema") != 1 or marker.get("complete") is not True or \
       validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Normalization capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Normalization capture differs from validated identity")
    events = [json.loads(line) for line in paths[1].read_text().splitlines()]
    rows, provenance = normalization_rows(events, marker)
    return rows, provenance, hashes
