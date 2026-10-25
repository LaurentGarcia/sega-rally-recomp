#!/usr/bin/env python3
"""Compare bounded native cross products with verified private geometry firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block, integer, saved_return

FIRMWARE_SHA256 = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
REGION_SHA256 = "fa2fb8f0f29928fda96ab2c57bed7bf65048cc86215ead05afc63d6b5680004f"
SOURCES = ["platform/ps2/native/rounded_cross_product.h", "platform/ps2/native/rounded_cross_product.cpp",
    "platform/ps2/tests/rounded_cross_product_cases.h", "platform/ps2/tests/rounded_cross_product_oracle.cpp",
    "platform/ps2/tools/check_rounded_cross_product.py", "platform/ps2/tools/check_contact_neighborhood.py",
    "platform/ps2/tools/check_driving_contact.py",
    "tools/m2tgprecomp/main.cpp", "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/fast_paths.h"]


def verify_firmware(image):
    if len(image) != 6660 or hashlib.sha256(image).hexdigest() != FIRMWARE_SHA256:
        raise ValueError("firmware does not match the verified private image")
    if hashlib.sha256(image[0x55f*4:0x577*4]).hexdigest() != REGION_SHA256:
        raise ValueError("cross-product instruction region differs from verified identity")


def ordinary(address, size):
    return not address & 3 and ((0x200000 <= address <= 0x240000-size) or
                               (0x500000 <= address <= 0x600000-size))


def cross_rows(events, marker):
    """Bind every cross pair to a complete sampled parent and its saved caller."""
    kinds = {"main_entry", "main_prequery", "query_pre", "query_count", "query_post",
             "contact_pre", "contact_reply", "contact_post", "response_post",
             "normalization_pre", "normalization_post", "metric_post", "step_post",
             "aggregate_pre", "aggregate_post", "cross_pre", "cross_post", "main_return"}
    parent = None
    pending = None
    rows, provenance, seen = [], [], set()
    last_instruction = -1
    for event in events:
        if not isinstance(event, dict) or event.get("schema") != 1 or event.get("event") not in kinds:
            raise ValueError("Unsupported main-contact event/schema")
        kind = event["event"]
        call, obj = integer(event.get("call_id"),64), integer(event.get("object"),32)
        frame, instruction = integer(event.get("frame"),64), integer(event.get("instructions"),64)
        fp, pc = integer(event.get("main_frame_pointer"),32), integer(event.get("pc"),32)
        integer(event.get("ac"),32)
        regs, floats, words = event.get("registers"), event.get("fp_bits"), event.get("object_words")
        if not isinstance(regs,list) or len(regs)!=32 or not isinstance(floats,list) or len(floats)!=4 or \
           not isinstance(words,list) or len(words)!=256:
            raise ValueError("Main-contact event requires complete register/object state")
        for value in regs+words:
            integer(value,32)
        for value in floats:
            integer(value,64)
        if not call or not ordinary(obj,1024) or not ordinary(fp,0x114) or fp & 63 or instruction<=last_instruction:
            raise ValueError("Invalid main-contact identity, memory range or event order")
        last_instruction = instruction
        if kind == "main_entry":
            if parent is not None or call in seen or len(seen)>=32 or pc!=0x5d3380 or regs[16]!=obj or regs[31]!=fp:
                raise ValueError("Nested, duplicate or invalid main-contact entry")
            if not isinstance(event.get("return_frame"),dict):
                raise ValueError("Main-contact entry lacks actual saved caller proof")
            return_pc, caller_fp = saved_return(event)
            if return_pc!=0x5d421c or event.get("parent_return_frame")!=event["return_frame"]:
                raise ValueError("Main-contact entry differs from verified caller")
            parent={"call":call,"object":obj,"fp":fp,"frame":frame,"proof":event["return_frame"],
                    "return_pc":return_pc,"caller_fp":caller_fp,"pairs":0}
            continue
        if parent is None or call!=parent["call"] or obj!=parent["object"] or fp!=parent["fp"] or \
           frame<parent["frame"] or event.get("parent_return_frame")!=parent["proof"]:
            raise ValueError("Main-contact event lacks its unchanged parent identity")
        if kind == "main_return":
            if pending is not None or parent["pairs"] not in (0,4) or pc!=parent["return_pc"] or \
               regs[31]!=parent["caller_fp"] or event.get("return_frame")!=parent["proof"]:
                raise ValueError("Main-contact return is incomplete or differs from its saved caller")
            seen.add(call)
            parent=None
            continue
        if not kind.startswith("cross_"):
            if pending is not None:
                raise ValueError("Unexpected nested event within a cross-product pair")
            continue
        index=integer(event.get("cross_index"),32)
        if index>=4 or index!=parent["pairs"] or regs[31]!=fp or regs[13]!=obj or regs[23]!=12*index or \
           regs[16:19]!=[fp+0xd8,fp+0xd4,fp+0xd0] or event.get("return_frame")!=parent["proof"]:
            raise ValueError("Cross-product frame/index/result pointers differ from original boundary")
        if saved_return(event)!=(parent["return_pc"],parent["caller_fp"]):
            raise ValueError("Cross-product saved caller no longer matches its parent")
        left=block(event,"cross_left",fp+0x70+12*index,3)
        right=block(event,"cross_right",fp+0xa0+12*index,3)
        output=block(event,"cross_result",fp+0xd0,3)
        if kind == "cross_pre":
            if pending is not None or pc!=0x5d3a88 or regs[21]!=fp+0x40+12*index or len(rows)>=16384:
                raise ValueError("Invalid or excessive cross-product input boundary")
            if any((value & 0x7fffffff)>0x49800000 for value in left+right):
                raise ValueError("Captured cross-product input exceeds the native finite domain")
            pending={"left":left,"right":right,"frame":frame,"instructions":instruction,"object_words":words}
            continue
        if pending is None or pc!=0x5d3b04 or left!=pending["left"] or right!=pending["right"] or \
           frame<pending["frame"] or words!=pending["object_words"]:
            raise ValueError("Unpaired cross-product output or modified input/object state")
        rows.append(tuple(left+right+output))
        provenance.append({"call_id":call,"object":obj,"index":index,"entry_frame":pending["frame"],
                           "exit_frame":frame,"instructions":instruction-pending["instructions"]})
        parent["pairs"]+=1
        pending=None
    if parent is not None or pending is not None or not rows:
        raise ValueError("Incomplete or empty live cross-product capture")
    calls=integer(marker.get("main_calls"),64)
    completions=integer(marker.get("main_completions"),64)
    samples=integer(marker.get("main_samples"),64)
    total_cross=integer(marker.get("main_cross_pairs"),64)
    if not calls or completions!=calls or not samples or samples!=len(seen) or samples>calls or \
       integer(marker.get("main_events"),64)!=len(events) or \
       integer(marker.get("main_sampled_cross_pairs"),64)!=len(rows) or \
       total_cross%4 or not len(rows)<=total_cross<=4*calls:
        raise ValueError("Main-contact capture completion/sample/cross counts differ")
    for prefix in ("main_quer", "main_contact"):
        entry_key="main_queries" if prefix=="main_quer" else "main_contacts"
        completion_key="main_query_completions" if prefix=="main_quer" else "main_contact_completions"
        if integer(marker.get(entry_key),64)!=4*calls or integer(marker.get(completion_key),64)!=4*calls:
            raise ValueError("Main-contact call lacks its four completed course queries/results")
    return rows,provenance


def capture_rows(directory):
    directory=private_output(Path(directory))
    paths=[directory/"capture.json",directory/"main-contact.jsonl",directory.parent/"validation.json"]
    for path,limit in zip(paths,(16384,32*1024*1024,1024*1024)):
        if path.stat().st_size>limit:
            raise ValueError("Cross-product capture exceeds bounded reader size")
    hashes={str(path):digest(path) for path in paths}
    marker,validation=json.loads(paths[0].read_text()),json.loads(paths[2].read_text())
    if marker.get("schema")!=1 or marker.get("complete") is not True or \
       validation.get("status")!="passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Cross-product capture lacks complete observer-parity validation")
    for path in paths[:2]:
        if validation.get("capture_files_sha256",{}).get(path.name)!=hashes[str(path)]:
            raise ValueError("Cross-product capture differs from validated identity")
    rows,pairs=cross_rows([json.loads(line) for line in paths[1].read_text().splitlines()],marker)
    return rows,pairs,hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tgp-program", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    firmware = args.tgp_program.resolve()
    try:
        verify_firmware(firmware.read_bytes())
        output = private_output(args.output)
    except ValueError as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT / name) for name in SOURCES}
    rows,pairs,capture_hashes=capture_rows(args.capture) if args.capture else ([],[],{})
    output.mkdir(parents=True, exist_ok=False)
    generator, generated, executable = (output / x for x in ("m2tgprecomp", "private-tgp.cpp", "rounded-cross-oracle"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src")]
    commands = [common + [str(ROOT / "tools/m2tgprecomp/main.cpp"), "-o", str(generator)],
        [str(generator), str(firmware), str(generated)],
        common + ["-DM2TGP_WITH_HOOK", "-I", str(ROOT / "platform/ps2/native"),
            str(ROOT / "platform/ps2/tests/rounded_cross_product_oracle.cpp"),
            str(ROOT / "platform/ps2/native/rounded_cross_product.cpp"), str(ROOT / "src/runtime/tgp.cpp"),
            str(generated), "-o", str(executable)]]
    report = {"schema": 1, "status": "failed", "firmware_sha256": FIRMWARE_SHA256,
        "word_region": [0x55f, 0x577], "region_sha256": REGION_SHA256,
        "sources_sha256": initial, "commands": commands,
        "scope": "Separately rounded cyclic products and differences within declared finite bounds",
        "capture_sha256":capture_hashes,"live_records":len(rows),"live_unique_inputs":len({row[:6] for row in rows}),
        "live_pairs":pairs,"live_comparison_verified": False,"physical_console_verified": False}
    try:
        for i, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (output / f"build-{i}.log").write_text(result.stdout + result.stderr)
            result.check_returncode()
        run=[str(executable),str(firmware)]
        if rows:
            records=output/"cross-records.bin"
            with records.open("xb") as stream:
                stream.write(b"RXCROS1\0"+struct.pack("<I",len(rows)))
                for row in rows:
                    stream.write(struct.pack("<9I",*row))
            report["records_sha256"]=digest(records)
            run.append(str(records))
        result = subprocess.run(run, capture_output=True, text=True, timeout=60)
        (output / "oracle.log").write_text(result.stdout + result.stderr)
        report.update(result=result.stdout.strip(), exit_code=result.returncode,
            executable_sha256=digest(executable), generated_sha256=digest(generated), generator_sha256=digest(generator))
        result.check_returncode()
        if "ROUNDED_CROSS_ORACLE PASS" not in result.stdout:
            raise RuntimeError("original comparison did not emit its completion marker")
        if initial != {name: digest(ROOT / name) for name in SOURCES} or digest(firmware) != FIRMWARE_SHA256 or \
           capture_hashes!={name:digest(Path(name)) for name in capture_hashes}:
            raise RuntimeError("source or private firmware changed during comparison")
        report["status"] = "passed"
        report["live_comparison_verified"] = bool(rows)
        print(result.stdout, end="")
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
