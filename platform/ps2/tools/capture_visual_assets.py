#!/usr/bin/env python3
"""Capture raw model/material/camera evidence through an owned-ROM host replay."""
import argparse
import json
from pathlib import Path
import subprocess
import time
from capture_driving import ROOT, module, digest, PROGRAM_SHA256

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native-build',type=Path,default=ROOT/'build/ps2/native')
    parser.add_argument('--pack',type=Path,default=ROOT/'build/ps2/assets/game.rpk')
    parser.add_argument('--inputs',type=Path,default=ROOT/'config/segarally_lancia_drive.inputs')
    parser.add_argument('--first',type=int,default=2399);parser.add_argument('--last',type=int,default=2401)
    parser.add_argument('--frames',type=int,default=2850);parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--reuse-host',type=Path,help='reuse a prior identical CMake source inventory under build/')
    args=parser.parse_args();output=args.output.resolve();native=args.native_build.resolve()
    build=ROOT/'build';pack=args.pack.resolve();inputs=args.inputs.resolve()
    if build.is_symlink() or output==build or not output.is_relative_to(build) or output.exists():parser.error('new real build output directory required')
    if not 0<args.first<=args.last<args.frames<=10000 or args.last-args.first>8 or not 1<=args.jobs<=8:parser.error('invalid bounded capture interval')
    prepare=module('platform/ps2/prepare_core.py','visual_prepare');generated,provenance=prepare.validate_native_build(native)
    packing=module('platform/ps2/tools/asset_pack.py','visual_pack');reader=packing.PackReader(pack)
    try:
        reader.verify();regions=reader.regions
        if regions['program']['sha256']!=PROGRAM_SHA256:raise ValueError('Revision B program identity differs')
        for name,(audit,key) in {'program':('coverage.json','program'),'main_data':('coverage.json','main_data'),
            'tgp_tables':('tgp/audit.json','copro_tgp_tables'),'copro_data':('tgp/audit.json','copro_data'),
            'polygons':('video/audit.json','polygons'),'textures':('video/audit.json','textures')}.items():
            if regions[name]['sha256']!=json.loads((native/audit).read_text())['images'][key]['sha256']:raise ValueError('Pack/audit mismatch: '+name)
    finally:reader.close()
    helper=module('platform/ps2/build_ps2.py','visual_build')
    sources=[ROOT/'platform/ps2/tools'/n for n in ('visual_capture_main.cpp','visual_asset_capture.cpp','visual_camera_capture.cpp','mesh_capture.cpp','mesh_decode.cpp')]
    sources+=generated+[ROOT/'src/runtime'/(n+'.cpp') for n in prepare.RUNTIME_SOURCES]
    source_inputs=set(sources+helper.project_headers(sources,[ROOT/'src',ROOT]))
    source_inputs.update(ROOT/n for n in ('platform/ps2/tools/capture_visual_assets.py','platform/ps2/tools/capture_driving.py',
        'platform/ps2/prepare_core.py','platform/ps2/build_ps2.py','platform/ps2/tools/asset_pack.py','platform/segarally/stage_data.py'))
    hashes={str(p.relative_to(ROOT)):digest(p) for p in sorted(source_inputs)}
    identities={k:{'path':str(p),'sha256':digest(p)} for k,p in (('pack',pack),('inputs',inputs))}
    output.mkdir(parents=True)
    cmake=['cmake_minimum_required(VERSION 3.20)','project(visual_reference LANGUAGES CXX)','set(CMAKE_CXX_STANDARD 20)','add_executable(visual-reference']
    cmake +=[json.dumps(str(p)) for p in sources]+[')', 'target_include_directories(visual-reference PRIVATE '+json.dumps(str(ROOT/'src'))+' '+json.dumps(str(ROOT))+')',
        'target_compile_definitions(visual-reference PRIVATE '+' '.join(d for d in prepare.DEFINITIONS if d!='RALLY_PS2_EE=1')+')',
        'target_compile_options(visual-reference PRIVATE -O2 -fno-fast-math -ffp-contract=off)',
        'set_source_files_properties('+json.dumps(str(ROOT/'src/runtime/geo.cpp'))+' '+json.dumps(str(ROOT/'src/runtime/model2a_machine.cpp'))+' PROPERTIES COMPILE_DEFINITIONS RALLY_OFFLINE_VISUAL_CAPTURE=1)']
    (output/'CMakeLists.txt').write_text('\n'.join(cmake)+'\n')
    host=output/'host'
    commands=[['cmake','-S',str(output),'-B',str(host),'-DCMAKE_BUILD_TYPE=Release']]
    if args.reuse_host:
        host=args.reuse_host.resolve()
        if not host.is_relative_to(build) or host==build or not (host/'CMakeCache.txt').is_file():raise ValueError('reuse-host must be an existing confined CMake build')
        cache=(host/'CMakeCache.txt').read_text()
        homes=[line.split('=',1)[1] for line in cache.splitlines() if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')]
        if len(homes)!=1 or not Path(homes[0]).resolve().is_relative_to(build) or (Path(homes[0])/'CMakeLists.txt').read_text()!='\n'.join(cmake)+'\n':raise ValueError('reuse-host CMake source inventory differs')
        commands=[]
    commands.append(['cmake','--build',str(host),'-j',str(args.jobs)])
    report={'schema':1,'status':'incomplete','scope':'Offline original execution and graphics extraction; not native gameplay',
        'frames':args.frames,'first':args.first,'last':args.last,'sources_sha256':hashes,'inputs':identities,'generation':provenance,'pack_regions':regions}
    started=time.monotonic()
    try:
        for i,command in enumerate(commands):
            with (output/f'build-{i}.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
        exe=host/'visual-reference';report['executable_sha256']=digest(exe);report['host_build']=str(host.relative_to(ROOT))
        print('Visual host compiled; starting capture and exact disabled baseline',flush=True)
        for name,directory in (('captured',str(output/'capture')),('baseline','-')):
            command=[str(exe),str(pack),str(inputs),str(args.frames),directory,str(args.first),str(args.last)]
            with (output/(name+'.csv')).open('w') as out,(output/(name+'.log')).open('w') as err:
                r=subprocess.run(command,stdout=out,stderr=err,timeout=900)
            report[name+'_returncode']=r.returncode
            if r.returncode:raise ValueError(name+' visual run failed; preserve logs')
            print(name+' complete',flush=True)
        helper.verify_source_snapshot(hashes)
        if digest(exe)!=report['executable_sha256'] or any(digest(p)!=identities[k]['sha256'] for k,p in (('pack',pack),('inputs',inputs))):raise ValueError('run inputs changed')
        a=(output/'captured.csv').read_bytes();b=(output/'baseline.csv').read_bytes()
        if a!=b or len(a.splitlines())!=args.frames+1:raise ValueError('observer changed/incompleted frame checkpoints')
        markers=[json.loads((output/f'capture/frame-{frame}/visual.json').read_text()) for frame in range(args.first,args.last+1)]
        if any(m.get('complete') is not True or not m['objects'] or not m['links'] for m in markers):raise ValueError('incomplete visual material capture')
        camera=json.loads((output/'capture/camera/capture.json').read_text())
        if camera.get('complete') is not True or not camera['calls'] or camera['calls']!=camera['completions']:raise ValueError('incomplete camera capture')
        report.update(status='passed',exact_checkpoint_match=True,matched_frames=args.frames,camera=camera,visual_frames=markers,
            checkpoint_sha256=digest(output/'captured.csv'),capture_files_sha256={str(p.relative_to(output/'capture')):digest(p) for p in sorted((output/'capture').rglob('*')) if p.is_file()})
    except Exception as e:report.update(status='failed',error=str(e));raise
    finally:
        report['elapsed_seconds']=round(time.monotonic()-started,3);(output/'validation.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print(json.dumps({k:report[k] for k in ('status','matched_frames','visual_frames','camera','elapsed_seconds')},indent=2))
if __name__=='__main__':main()
