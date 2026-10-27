#!/usr/bin/env python3
"""Compare the native persistent vehicle updates with one sealed original seed."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import hashlib

from check_contact_neighborhood import ROOT,digest,private_output
from extract_main_vehicle import SOURCES as EXTRACT_SOURCES

NATIVE=('angle_fold', 'cabinet_steering', 'cabinet_steering_filter', 'candidate_contact_pass', 'contact_aggregate', 'contact_neighborhood', 'contact_normalization', 'contact_pass_response', 'contact_point_preparation', 'contact_projection', 'contact_response', 'contact_result', 'contact_search', 'driving_assets', 'main_balance_geometry', 'main_balance_math', 'main_balance_update', 'main_candidate', 'main_car_pose', 'main_contact_slot', 'main_contact_step', 'main_front_update', 'main_motion_update', 'main_update_entry', 'main_vehicle_checkpoint', 'main_vehicle_update', 'paired_balance', 'recovered_atangent', 'recovered_scalar_metric', 'rounded_affine_transform', 'rounded_cross_product', 'scene_contact_damping', 'segment_selection', 'vehicle_arithmetic', 'vehicle_parameter_selection')


def source_closure():
    sources={ROOT/f"platform/ps2/native/{name}.cpp" for name in NATIVE}
    sources.add(ROOT/"platform/ps2/tests/main_vehicle_update_live.cpp")
    pending=list(sources)
    while pending:
        path=pending.pop()
        for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"',path.read_text(),re.MULTILINE):
            candidates=(path.parent/include,ROOT/"platform/ps2/native"/include,ROOT/"src"/include,ROOT/include)
            dependency=next((p.resolve() for p in candidates if p.is_file()),None)
            if dependency is None or not dependency.is_relative_to(ROOT): raise ValueError("Unresolved vehicle proof header: "+include)
            if dependency not in sources: sources.add(dependency);pending.append(dependency)
    sources|={ROOT/p for p in EXTRACT_SOURCES}
    sources.add(Path(__file__).resolve())
    return sorted(str(p.relative_to(ROOT)) for p in sources)


def validated_records(path):
    path=private_output(path)
    if not path.is_file() or not 12<=path.stat().st_size<=16*1024*1024: raise ValueError("Vehicle records exceed bounds")
    with path.open("rb") as stream: header=stream.read(12)
    if header[:8]!=b"VEHSTEP1": raise ValueError("Vehicle record format differs")
    count=struct.unpack_from("<I",header,8)[0]
    if not 1<=count<=1201: raise ValueError("Vehicle record count exceeds bounds")
    report=path.parent/"result.json"
    if report.is_symlink() or not report.is_file() or report.stat().st_size>4*1024*1024:
        raise ValueError("Vehicle extraction report is missing or oversized")
    evidence=json.loads(report.read_text())
    if evidence.get("status")!="passed" or evidence.get("record_count")!=count or evidence.get("records_sha256")!=digest(path):
        raise ValueError("Vehicle records differ from their continuous replay extraction")
    hashes=evidence.get("capture_sha256")
    if not isinstance(hashes,dict) or not hashes or not any(p.endswith("full-step-access.bin") for p in hashes):
        raise ValueError("Vehicle extraction lacks its complete original access identity")
    for name,expected in hashes.items():
        if digest(private_output(Path(name)))!=expected: raise ValueError("Original capture changed since vehicle extraction")
    required=set(EXTRACT_SOURCES)
    actual=evidence.get("sources_sha256",{})
    if not required<=actual.keys() or any(digest(ROOT/name)!=actual[name] for name in required):
        raise ValueError("Vehicle extraction source closure differs")
    return path,count,report,hashes


def negative_cases(executable,assets,records,output):
    """Exercise the real checker against bounded mutations of sealed evidence."""
    original=records.read_bytes();seed_size=struct.unpack_from("<I",original,12)[0]
    at=16+seed_size;rows=[]
    for _ in range(2):
        start=at;frame,fp,obj,pose,_=struct.unpack_from("<5I",original,at);at+=20
        controls=at;at+=12*4+3*4
        count=struct.unpack_from("<I",original,at)[0];at+=4;fields={}
        for _ in range(count):
            address,n=struct.unpack_from("<2I",original,at);at+=8
            fields.update({address+4*i:at+4*i for i in range(n)});at+=4*n
        n=struct.unpack_from("<I",original,at)[0];at+=4+8*n
        rows.append((start,controls,fields,obj,pose))
    def changed(at,mask):
        b=bytearray(original);word=struct.unpack_from("<I",b,at)[0]
        struct.pack_into("<I",b,at,word^mask);return bytes(b)
    cases=(
        ("invalid_seed_size",changed(12,4),"Invalid vehicle seed/count"),
        ("truncated_final_record",original[:-1],"Truncated vehicle record"),
        ("trailing_data",original+b"\0\0\0\0","Extra vehicle record bytes"),
        ("altered_expected_pose",changed(rows[0][2][rows[0][4]],1),"Published dynamic pose differs"),
        ("changed_second_frame_control",changed(rows[1][1]+7*4,0x40),"Persistent native state differs"),
        ("nonconsecutive_frame",changed(rows[1][0],4),"Invalid consecutive vehicle identity"),
    )
    evidence=[]
    for name,data,reason in cases:
        with tempfile.TemporaryDirectory(prefix=name+"-",dir=output) as directory:
            work=Path(directory);path=work/"mutated.bin";path.write_bytes(data)
            result=subprocess.run([str(executable),str(assets),str(path),str(work)],capture_output=True,text=True,timeout=60)
            (output/(name+".log")).write_text(result.stdout+result.stderr)
            if result.returncode==0 or reason not in result.stderr or any(work.glob("*-car.bin")):
                raise ValueError("Vehicle negative case did not fail at the required boundary: "+name)
            evidence.append({"case":name,"mutated_sha256":hashlib.sha256(data).hexdigest(),"exit_code":result.returncode,"reason":reason})
    return evidence


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--records",type=Path,required=True);parser.add_argument("--assets",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True);parser.add_argument("--sanitize",action="store_true");parser.add_argument("--negative-checks",action="store_true")
    args=parser.parse_args();compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler: parser.error("C++20 compiler required")
    try:
        records,count,extraction,capture_hashes=validated_records(args.records)
        assets=private_output(args.assets);output=private_output(args.output)
        if not assets.is_file() or assets.stat().st_size>16*1024*1024: raise ValueError("Driving asset package is missing or oversized")
        initial={name:digest(ROOT/name) for name in source_closure()}
        inputs={str(p):digest(p) for p in (records,assets,extraction)}
    except (OSError,ValueError) as error: parser.error(str(error))
    output.mkdir(parents=True,exist_ok=False);exe=output/"vehicle-live"
    command=[compiler,"-std=c++20","-O1" if args.sanitize else "-O2","-Wall","-Wextra","-Werror",
             "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native")]
    if args.sanitize: command += ["-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    command += [*(str(ROOT/f"platform/ps2/native/{s}.cpp") for s in NATIVE),
                str(ROOT/"platform/ps2/tests/main_vehicle_update_live.cpp"),"-o",str(exe)]
    report={"schema":1,"status":"failed","sources_sha256":initial,"private_inputs_sha256":inputs,
            "capture_sha256":capture_hashes,"build_command":command,"sanitizers":args.sanitize,
            "scope":"One initial native state seed, uninterrupted self-fed callbacks with explicit external inputs; canonical state, published pose and ordered sound compared",
            "canonical_fields_per_frame":161,"pose_words_per_frame":22,"original_state_seed_count":1,
            "excluded_from_canonical_equality":["stack temporaries and original pointers","CPU/IRQ/device state","sound queue transport ownership"],
            "successive_native_state_proven":False,"native_gameplay_connected":False,"physical_console_verified":False}
    try:
        result=subprocess.run(command,capture_output=True,text=True,timeout=90)
        (output/"build.log").write_text(result.stdout+result.stderr);result.check_returncode()
        result=subprocess.run([str(exe),str(assets),str(records),str(output)],capture_output=True,text=True,timeout=60)
        (output/"check.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(),exit_code=result.returncode,executable_sha256=digest(exe));result.check_returncode()
        if not re.fullmatch(rf"MAIN_VEHICLE_UPDATE_LIVE PASS steps={count} state_words={161*count} pose_words={22*count} sound_requests=[0-9]+ seeds=1 hash=[0-9a-f]{{16}}\n",result.stdout):
            raise ValueError("Vehicle checker lacks the expected complete result marker")
        if args.negative_checks: report["negative_checks"]=negative_cases(exe,assets,records,output)
        if initial!={name:digest(ROOT/name) for name in initial} or inputs!={name:digest(Path(name)) for name in inputs} or \
           any(digest(Path(name))!=value for name,value in capture_hashes.items()): raise ValueError("Vehicle proof inputs changed")
        report["checkpoint_sha256"]={name:digest(output/name) for name in ("initial-car.bin","driving-car.bin")};report["status"]="passed";report["successive_native_state_proven"]=True;print(result.stdout,end="")
    finally: (output/"result.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")


if __name__=="__main__": main()
