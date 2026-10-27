#!/usr/bin/env python3
"""Compare the native input/selection wrapper with sealed original entry states."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT,digest,private_output
from extract_main_stages import SOURCES as EXTRACT_SOURCES

NATIVE=("main_update_entry","segment_selection","contact_projection","cabinet_steering_filter","cabinet_steering","driving_assets")


def source_closure():
    sources={ROOT/f"platform/ps2/native/{name}.cpp" for name in NATIVE}
    sources.add(ROOT/"platform/ps2/tests/main_update_entry_live.cpp")
    pending=list(sources)
    while pending:
        path=pending.pop()
        for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"',path.read_text(),re.MULTILINE):
            candidates=(path.parent/include,ROOT/"platform/ps2/native"/include,ROOT/"src"/include,ROOT/include)
            dependency=next((p.resolve() for p in candidates if p.is_file()),None)
            if dependency is None or not dependency.is_relative_to(ROOT): raise ValueError("Unresolved entry proof header: "+include)
            if dependency not in sources: sources.add(dependency);pending.append(dependency)
    sources|={ROOT/p for p in EXTRACT_SOURCES}
    sources.add(Path(__file__).resolve())
    return sorted(str(p.relative_to(ROOT)) for p in sources)


def validated_records(path):
    path=private_output(path)
    if not path.is_file() or not 12<=path.stat().st_size<=16*1024*1024: raise ValueError("Entry records exceed bounds")
    with path.open("rb") as stream: header=stream.read(12)
    if header[:8]!=b"ENTSTEP1": raise ValueError("Entry record format differs")
    count=struct.unpack_from("<I",header,8)[0]
    if not 1<=count<=1201: raise ValueError("Entry record count exceeds bounds")
    report=path.parent/"result.json"
    if report.is_symlink() or not report.is_file() or report.stat().st_size>4*1024*1024:
        raise ValueError("Entry extraction report is missing or oversized")
    evidence=json.loads(report.read_text())
    if evidence.get("status")!="passed" or evidence.get("record_count")!=count or evidence.get("entry_sha256")!=digest(path):
        raise ValueError("Entry records differ from their continuous replay extraction")
    hashes=evidence.get("capture_sha256")
    if not isinstance(hashes,dict) or not hashes or not any(p.endswith("full-step-access.bin") for p in hashes):
        raise ValueError("Entry extraction lacks its complete original access identity")
    for name,expected in hashes.items():
        if digest(private_output(Path(name)))!=expected: raise ValueError("Original capture changed since entry extraction")
    required=set(EXTRACT_SOURCES)
    actual=evidence.get("sources_sha256",{})
    if not required<=actual.keys() or any(digest(ROOT/name)!=actual[name] for name in required):
        raise ValueError("Entry extraction source closure differs")
    return path,count,report,hashes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--records",type=Path,required=True);parser.add_argument("--assets",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True);parser.add_argument("--sanitize",action="store_true")
    args=parser.parse_args();compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    try:
        records,count,extraction,capture_hashes=validated_records(args.records)
        assets=private_output(args.assets);output=private_output(args.output)
        if not assets.is_file() or assets.stat().st_size>16*1024*1024: raise ValueError("Driving asset package is missing or oversized")
        initial={name:digest(ROOT/name) for name in source_closure()}
        inputs={str(p):digest(p) for p in (records,assets,extraction)}
    except (OSError,ValueError) as error: parser.error(str(error))
    output.mkdir(parents=True,exist_ok=False);exe=output/"entry-live"
    command=[compiler,"-std=c++20","-O1" if args.sanitize else "-O2","-Wall","-Wextra","-Werror",
             "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native")]
    if args.sanitize: command += ["-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    command += [*(str(ROOT/f"platform/ps2/native/{s}.cpp") for s in NATIVE),
                str(ROOT/"platform/ps2/tests/main_update_entry_live.cpp"),"-o",str(exe)]
    report={"schema":1,"status":"failed","sources_sha256":initial,"private_inputs_sha256":inputs,
            "capture_sha256":capture_hashes,"build_command":command,"sanitizers":args.sanitize,
            "scope":"Original34f40..34ff0 entry stage, independent per-stage inputs; full native vehicle continuity not established here",
            "successive_native_state_proven":False,"native_gameplay_connected":False,"physical_console_verified":False}
    try:
        result=subprocess.run(command,capture_output=True,text=True,timeout=90)
        (output/"build.log").write_text(result.stdout+result.stderr);result.check_returncode()
        result=subprocess.run([str(exe),str(assets),str(records)],capture_output=True,text=True,timeout=60)
        (output/"check.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(exe));result.check_returncode()
        if not re.fullmatch(rf"MAIN_UPDATE_ENTRY_LIVE PASS steps={count} state_words={6*count} hash=[0-9a-f]{{16}}\n",result.stdout):
            raise ValueError("Entry checker lacks the expected complete result marker")
        if initial!={name:digest(ROOT/name) for name in initial} or inputs!={name:digest(Path(name)) for name in inputs} or \
           any(digest(Path(name))!=value for name,value in capture_hashes.items()): raise ValueError("Entry proof inputs changed")
        report["status"]="passed";print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")


if __name__=="__main__": main()
