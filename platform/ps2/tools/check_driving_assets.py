#!/usr/bin/env python3
"""Verify native package bytes and loaded-asset contact steps against sealed data."""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from export_driving_assets import ROOT, INPUTS, digest, export
from check_main_contact_step import NATIVE, SOURCES as STEP_SOURCES, read_capture, verify_inputs

SOURCES = list(dict.fromkeys([*STEP_SOURCES,
    "platform/ps2/native/driving_assets.h", "platform/ps2/native/driving_assets.cpp",
    "platform/ps2/native/main_balance_update.h", "platform/ps2/native/main_balance_geometry.h",
    "platform/ps2/native/segment_selection.h", "platform/ps2/native/segment_selection.cpp",
    "platform/ps2/native/vehicle_parameter_selection.h", "platform/ps2/native/vehicle_parameter_selection.cpp",
    "platform/ps2/tests/driving_assets_test.cpp", "platform/ps2/tests/driving_assets_live.cpp",
    "platform/ps2/tools/export_driving_assets.py", "platform/ps2/tools/check_driving_assets.py",
    "tests/test_ps2_driving_assets.py"]))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in (*INPUTS,"capture","output"):
        parser.add_argument("--"+key.replace("_","-"),type=Path,required=True)
    args=parser.parse_args()
    build=ROOT/"build"
    output=args.output.absolute()
    if any(p.is_symlink() for p in (output,*output.parents)) or not output.resolve().is_relative_to(build.resolve()) or output.exists():
        parser.error("output must be a new directory below ignored build/ without symlinks")
    compiler=shutil.which("clang++") or shutil.which("c++")
    if not compiler:parser.error("C++20 compiler required")
    paths={key:getattr(args,key).resolve() for key in INPUTS}
    local=verify_inputs({"program":paths["program"],"tgp_tables":paths["tables"],"copro_data":paths["copro"]})
    rows,provenance,capture_hashes=read_capture(args.capture,local)
    selectors={row[104] for row in rows}
    if len(selectors)!=1:parser.error("this bounded integration proof requires one selected course")
    selector=next(iter(selectors))
    initial={name:digest((ROOT/name).read_bytes()) for name in SOURCES}
    output.mkdir(parents=True)
    report={"schema":1,"status":"failed","scope":"native contact/selection asset transport and composed contact-step integration",
            "sources_sha256":initial,"capture_sha256":capture_hashes,"live_steps":len(rows),"live_pairs":provenance,
            "native_gameplay_connected":False,"physical_console_verified":False,
            "limitations":["No complete vehicle initialization or continuous update",
                           "Contact scalar parameters remain explicit observed vehicle-state inputs; thirteen-word package records belong to a different helper",
                           "Balance/power/motion assets are transported exactly; their full vehicle update is a separate validation task",
                           "No rendering mesh or camera assets;189 sound rows cover demonstrated initialized codes through188",
                           "All eight packages checked; live contact-step integration covers only the captured selector"]}
    try:
        native=ROOT/"platform/ps2/native"
        base=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(native),"-I",str(ROOT/"platform/ps2/tests")]
        probe=output/"asset-probe";live=output/"asset-live"
        probe_names=("driving_assets","segment_selection","contact_projection","contact_search","contact_neighborhood","vehicle_parameter_selection")
        commands=[base+[str(ROOT/"platform/ps2/tests/driving_assets_test.cpp"),*(str(native/(n+".cpp")) for n in probe_names),"-o",str(probe)],
                  base+[str(ROOT/"platform/ps2/tests/driving_assets_live.cpp"),str(native/"driving_assets.cpp"),*(str(native/(n+".cpp")) for n in NATIVE),"-o",str(live)]]
        report["commands"]=commands
        for i,command in enumerate(commands):
            result=subprocess.run(command,capture_output=True,text=True,timeout=120)
            (output/f"compile-{i}.log").write_text(result.stdout+result.stderr);result.check_returncode()
        packages=[]
        for index in range(8):
            path=output/f"course-{index}.bin"
            metadata=export(paths,index,path)
            raw=path.read_bytes();fingerprint=14695981039346656037
            for byte in raw[96:]:fingerprint=((fingerprint^byte)*1099511628211)&((1<<64)-1)
            result=subprocess.run([str(probe),str(path)],capture_output=True,text=True,timeout=30)
            (output/f"course-{index}.log").write_text(result.stdout+result.stderr);result.check_returncode()
            match=re.fullmatch(r"DRIVING_ASSETS PASS checks=120 storage_bytes=\d+\nDRIVING_ASSETS FILE PASS selector=(\d+) vertices=(\d+) segments=(\d+) polygons=(\d+) crc=([0-9a-f]{8}) storage_bytes=(\d+) payload_hash=([0-9a-f]{16})\n",result.stdout)
            expected=metadata["output"]
            if not match or tuple(map(int,match.group(1,2,3,4)))!=(index,expected["vertices"],expected["segments"],expected["polygons"]) or match[5]!=expected["payload_crc32"] or match[7]!=f"{fingerprint:016x}":
                raise RuntimeError("loaded typed values do not match complete serialized payload")
            packages.append({**expected,"storage_bytes_host":int(match[6]),"typed_payload_hash":match[7]})
        records=output/"private-steps.bin"
        records.write_bytes(b"MAINSTP1"+struct.pack("<I",len(rows))+b"".join(struct.pack("<110I",*r) for r in rows))
        result=subprocess.run([str(live),str(output/f"course-{selector}.bin"),str(records)],capture_output=True,text=True,timeout=60)
        (output/"integration.log").write_text(result.stdout+result.stderr);result.check_returncode()
        match=re.fullmatch(r"DRIVING_ASSETS_LIVE PASS steps=(\d+) contacts=(\d+) active_steps=(\d+) state_words=(\d+) hash=([0-9a-f]{16})\n",result.stdout)
        if not match or tuple(map(int,match.group(1,2,4)))!=(len(rows),4*len(rows),46*len(rows)) or int(match[3])>len(rows):
            raise RuntimeError("integration completion marker invalid")
        if initial!={name:digest((ROOT/name).read_bytes()) for name in SOURCES} or any(digest(Path(p).read_bytes())!=sha for p,sha in capture_hashes.items()) or any(digest(paths[name].read_bytes())!=identity[1] for name,identity in INPUTS.items()):
            raise RuntimeError("source or input changed during comparison")
        report.update(status="passed",packages=packages,result=result.stdout.strip(),active_steps=int(match[3]),records_sha256=digest(records.read_bytes()),
                      inputs_sha256={name:value[1] for name,value in INPUTS.items()},executable_sha256={p.name:digest(p.read_bytes()) for p in (probe,live)})
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")
    print(report["result"])

if __name__=="__main__":main()
