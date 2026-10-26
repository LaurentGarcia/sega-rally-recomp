#!/usr/bin/env python3
"""Compare a typed native candidate pass with sealed private original boundaries."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from check_contact_count import IDENTITIES
from check_contact_neighborhood import ROOT, digest, private_output
from check_contact_pass_response import pass_rows, read_capture as response_capture
from check_contact_point_preparation import point_rows
from check_driving_contact import block, integer

NATIVE = ("candidate_contact_pass", "contact_point_preparation", "rounded_affine_transform",
          "contact_pass_response", "contact_response", "contact_normalization", "rounded_cross_product",
          "recovered_scalar_metric", "contact_aggregate", "contact_search", "contact_projection",
          "contact_neighborhood", "contact_result")
SOURCES = [f"platform/ps2/native/{name}.{ext}" for name in NATIVE for ext in ("cpp", "h")] + [
    "platform/ps2/tests/candidate_contact_pass_live.cpp", "platform/ps2/tests/contact_pass_response_cases.h",
    "platform/ps2/tools/check_candidate_contact_pass.py", "platform/ps2/tools/check_contact_pass_response.py",
    "platform/ps2/tools/check_contact_point_preparation.py", "platform/ps2/tools/check_contact_result.py",
    "platform/ps2/tools/check_contact_query.py", "platform/ps2/tools/check_contact_count.py",
    "platform/ps2/tools/check_contact_neighborhood.py", "platform/ps2/tools/check_driving_contact.py",
    "platform/ps2/tools/check_recovered_scalar_metric.py"]


def candidate_rows(events, marker, local_words):
    base, provenance = pass_rows(events, marker)
    point_records, point_provenance = point_rows(events, marker, local_words)
    if len(base) != len(point_records):
        raise ValueError("Candidate preparation and response populations differ")
    rows, parent = [], None
    for event in events:
        kind = event["event"]
        if kind == "main_entry":
            selector = block(event, "course_selector", 0x214354, 1)[0]
            segment_word = event["object_words"][34]
            segment = segment_word & 0xffff
            if selector >= 8 or segment >= 0x8000:
                raise ValueError("Candidate selector or signed segment exceeds recovered domain")
            parent = {"selector": selector, "segment_word": segment_word, "segment": segment,
                      "slots": block(event, "result_slots", 0x2138d0, 36),
                      "local": block(event, "local_contact_points", 0x5d2e30, 12)}
        if parent is None:
            raise ValueError("Candidate observation lacks a main parent")
        if block(event, "course_selector", 0x214354, 1) != [parent["selector"]] or \
           event["object_words"][34] != parent["segment_word"]:
            raise ValueError("Candidate course or segment changed during the pass")
        if kind == "main_prequery" and block(event, "result_slots", 0x2138d0, 36) != parent["slots"]:
            raise ValueError("Point preparation changed prior contact slots")
        if kind == "contact_pre":
            index = integer(event.get("contact_index"), 32)
            if index >= 4 or block(event, "contact_output", 0x2138d0+36*index, 8) != parent["slots"][9*index:9*index+8]:
                raise ValueError("Contact adapter prior state differs from entry slots")
        if kind == "main_return":
            index = len(rows)
            if index >= len(base) or point_provenance[index]["call_id"] != event["call_id"] or \
               provenance[index]["call_id"] != event["call_id"]:
                raise ValueError("Candidate component parent identities differ")
            slots = block(event, "result_slots", 0x2138d0, 36)
            old_contacts = []
            for slot in range(4):
                start = 9*slot
                if slots[start:start+3] != parent["slots"][start:start+3] or \
                   slots[start+8] != parent["slots"][start+8] or \
                   tuple(slots[start+3:start+8]) != base[index][5*slot:5*slot+5]:
                    raise ValueError("Final contact slots differ from paired adapter outputs or preserved fields")
                old_contacts.extend(parent["slots"][start+3:start+8])
            rows.append(base[index]+tuple(parent["local"])+
                        (parent["selector"],parent["segment"])+tuple(old_contacts))
            parent = None
    if parent is not None or len(rows) != len(base) or any(len(row) != 99 for row in rows):
        raise ValueError("Incomplete candidate pass records")
    return rows, provenance


def read_capture(directory, local_words):
    _, _, hashes = response_capture(directory)
    directory = private_output(Path(directory))
    marker = json.loads((directory/"capture.json").read_text())
    events = [json.loads(line) for line in (directory/"main-contact.jsonl").read_text().splitlines()]
    rows, provenance = candidate_rows(events, marker, local_words)
    if hashes != {name: digest(Path(name)) for name in hashes}:
        raise ValueError("Candidate capture changed during parsing")
    return rows, provenance, hashes


def verify_inputs(paths):
    for name, path in paths.items():
        size, expected = IDENTITIES[name]
        if path.stat().st_size != size or digest(path) != expected:
            raise ValueError(f"{name} differs from verified private identity")
    return struct.unpack("<12I", paths["program"].read_bytes()[0x33e30:0x33e60])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("program", "tables", "copro", "capture", "output"):
        parser.add_argument("--"+name, type=Path, required=True)
    args = parser.parse_args()
    paths = {"program": args.program.resolve(), "tgp_tables": args.tables.resolve(), "copro_data": args.copro.resolve()}
    try:
        local_words = verify_inputs(paths)
        output = private_output(args.output)
        rows, provenance, capture_hashes = read_capture(args.capture, local_words)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    executable, records = output/"candidate-pass-live", output/"private-passes.bin"
    command = [compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off",
               "-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/tests/candidate_contact_pass_live.cpp"),
               *(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE),"-o",str(executable)]
    report = {"schema":1,"status":"failed","scope":"Complete typed candidate contact pass versus captured original boundaries",
              "native_gameplay_connected":False,"physical_console_verified":False,"live_comparison_verified":False,
              "sources_sha256":initial,"input_sha256":{name:IDENTITIES[name][1] for name in paths},
              "input_paths":{name:str(path) for name,path in paths.items()},"capture_sha256":capture_hashes,
              "command":command,"live_passes":len(rows),"live_pairs":provenance,
              "limitations":["Recorded replay window, not every course or driving mode",
                             "Compares the existing reference, not independent arcade hardware",
                             "No original CPU, generated firmware or device runtime linked into this checker"]}
    try:
        built = subprocess.run(command,capture_output=True,text=True,timeout=120)
        (output/"build.log").write_text(built.stdout+built.stderr);built.check_returncode()
        records.write_bytes(b"CANDPAS1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<99I",*row) for row in rows))
        report["records_sha256"] = digest(records)
        tested = subprocess.run([str(executable),*(str(path) for path in paths.values()),str(records)],
                                capture_output=True,text=True,timeout=60)
        (output/"comparison.log").write_text(tested.stdout+tested.stderr)
        report.update(result=tested.stdout.strip(),exit_code=tested.returncode,executable_sha256=digest(executable))
        tested.check_returncode()
        match = re.fullmatch(r"CANDIDATE_CONTACT_PASS_LIVE PASS passes=(\d+) contacts=(\d+) active_passes=(\d+) state_words=(\d+) hash=([0-9a-f]{16})\n?",tested.stdout)
        if match is None or tuple(map(int,(match[1],match[2],match[4]))) != (len(rows),4*len(rows),33*len(rows)) or \
           int(match[3]) > len(rows):
            raise RuntimeError("Candidate checker lacks exact completion/population marker")
        if initial != {name:digest(ROOT/name) for name in SOURCES} or \
           capture_hashes != {name:digest(Path(name)) for name in capture_hashes} or \
           any(digest(path)!=IDENTITIES[name][1] for name,path in paths.items()) or digest(records)!=report["records_sha256"]:
            raise RuntimeError("Candidate source or private inputs changed during comparison")
        report.update(status="passed",live_comparison_verified=True,active_passes=int(match[3]))
        print(tested.stdout,end="")
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__=="__main__":main()
