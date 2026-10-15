#!/usr/bin/env python3
"""Build a local Windows/Linux player from personally supplied verified ROMs.

No player or ROM-bearing package is uploaded. Prerequisites are installed by the
user: Python 3.10+, Git, CMake 3.25+, a GNU-style C++20 compiler (clang++ on
Windows in a Visual Studio developer environment), and desktop GPU drivers.
Use --plan to inspect commands without changing files.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import signal
import subprocess
import sys
import time
import zipfile

ROOT=Path(__file__).resolve().parents[1]
DEPENDENCIES={
 'sdl3':('https://github.com/libsdl-org/SDL.git','fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'),
 'imgui':('https://github.com/ocornut/imgui.git','f1cc2ae15e53a861a874c3034aae6798fde194ab'),
 'lzma':('https://github.com/ip7z/7zip.git','0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17')}
TESTS=['test_segarally_audio','test_segarally_controls','test_segarally_display_preferences',
       'test_segarally_audio_output_capture','test_segarally_input_replay','test_segarally_platform_files']

def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for data in iter(lambda:f.read(65536),b''):h.update(data)
 return h.hexdigest()

def output_path(path):
 p=path.expanduser().resolve()
 if p.exists() or p==ROOT/'build' or not p.is_relative_to(ROOT/'build'):
  raise ValueError('Choose a NEW directory below this checkout\'s ignored build/; old builds/saves are preserved')
 return p

def build_plan(roms,out,jobs,compiler):
 native,host=out/'native',out/'host'
 cc=shlex.join([compiler]) # The existing generators parse --compiler with shlex, including paths with spaces.
 def script(name,*args):return [sys.executable,str(ROOT/name),*map(str,args)]
 return [
 ('main',script('scripts/segarally_recompile.py','--roms',roms,'--build-dir',native,'--jobs',jobs,
               '--seed-profile','lift-labels','--indexed-dispatch','--instruction-boundary-hook','--compiler',cc),900),
 ('tgp',script('scripts/segarally_tgp.py','--roms',roms,'--build-dir',native,'--compiler',cc),900),
 ('video',script('scripts/segarally_video.py','--roms',roms,'--build-dir',native),300),
 ('sound-import',script('scripts/segarally_sound.py','--roms',roms,'--build-dir',native),300),
 ('sound',script('scripts/segarally_sound_recompile.py','--build-dir',native,'--timed','--max-instructions',1000000,'--compiler',cc),900),
 ('configure',['cmake','-S',str(ROOT/'platform/segarally'),'-B',str(host),'-DCMAKE_BUILD_TYPE=Release',
               f'-DSEGARALLY_NATIVE_BUILD={native}',f'-DSEGARALLY_SOUND_BUILD={native}',
               '-DSEGARALLY_SOUND=ON','-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON','-DSEGARALLY_INPUT_RECORDING=ON'],300),
 ('compile',['cmake','--build',str(host),'--config','Release','--parallel',str(jobs),'--target','segarally_native',*TESTS],1200),
 ('tests',['ctest','--test-dir',str(host),'-C','Release','--output-on-failure'],300)]

def terminate(child):
 if os.name=='nt':
  # Windows CREATE_NEW_PROCESS_GROUP alone does not terminate descendants.
  subprocess.run(['taskkill','/PID',str(child.pid),'/T','/F'],capture_output=True,timeout=15)
  child.wait(timeout=15)
 else:
  try:os.killpg(child.pid,signal.SIGTERM)
  except ProcessLookupError:pass
  try:child.wait(timeout=5)
  except subprocess.TimeoutExpired:
   os.killpg(child.pid,signal.SIGKILL);child.wait()

def run(command,log,timeout):
 with log.open('wb') as stream:
  kwargs={'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=='nt' else {'start_new_session':True}
  child=subprocess.Popen(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,**kwargs)
  try:
   code=child.wait(timeout=timeout)
   if code:raise subprocess.CalledProcessError(code,command)
  except BaseException:
   if child.poll() is None:terminate(child)
   raise

def dependencies(fetch):
 for name,(url,revision) in DEPENDENCIES.items():
  folder=ROOT/'extern'/name
  if not folder.exists():
   if not fetch:raise ValueError(f'Missing extern/{name}; use --fetch-dependencies to download pinned public source')
   folder.parent.mkdir(exist_ok=True)
   for cmd in (['git','init',str(folder)],['git','-C',str(folder),'remote','add','origin',url],
               ['git','-C',str(folder),'fetch','--depth','1','origin',revision],
               ['git','-C',str(folder),'checkout','--detach',revision]):
    subprocess.run(cmd,check=True,timeout=600)
  actual=subprocess.check_output(['git','-C',str(folder),'rev-parse','HEAD'],text=True).strip()
  dirty=subprocess.check_output(['git','-C',str(folder),'status','--porcelain','--untracked-files=no'],text=True).strip()
  if actual!=revision or dirty:raise ValueError(f'extern/{name} must be the unmodified pinned revision; existing files were preserved')

def package(out):
 host=out/'host';windows=platform.system()=='Windows';name='segarally_native.exe' if windows else 'segarally_native'
 candidates=[p for p in (host/name,host/'Release'/name) if p.is_file()]
 if len(candidates)!=1:raise ValueError('Cannot identify the unique built player executable')
 exe=candidates[0]
 payload=out/'player';payload.mkdir()
 shutil.copy2(exe,payload/name)
 shutil.copytree(exe.parent/'assets',payload/'assets')
 shutil.copytree(host/'licenses',payload/'licenses')
 if windows:
  (payload/'Launch.cmd').write_text('@echo off\r\n"%~dp0segarally_native.exe" --player %*\r\n')
 else:
  launcher=payload/'launch.sh';launcher.write_text('#!/bin/sh\ncd -- "$(dirname -- "$0")" || exit 1\nexec ./segarally_native --player "$@"\n');launcher.chmod(0o755)
 (payload/'README.txt').write_text('Local personal game build. Do not upload or redistribute ROM-bearing outputs.\n'
   'Use Launch.cmd (Windows) or launch.sh (Linux); Vulkan-capable drivers required.\n'
   'Windows also requires the matching Microsoft Visual C++ runtime. Linux requires compatible system libraries.\n'
   'This ZIP is not a tested universal/static portable package; no game or GPU validation is implied by successful compilation.\n')
 manifest={str(p.relative_to(payload)): {'size':p.stat().st_size,'sha256':digest(p)} for p in sorted(payload.rglob('*')) if p.is_file()}
 archive=out/f'Sega-Rally-Native-local-{platform.system()}-{platform.machine()}.zip'
 with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED) as z:
  for rel in manifest:z.write(payload/rel,arcname=f'Sega Rally Native/{rel}')
 return {'archive':str(archive),'archive_sha256':digest(archive),'files':manifest}

def main(argv=None):
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--roms',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--jobs',type=int,default=2,choices=range(1,9));p.add_argument('--generation-compiler',default='clang++' if os.name=='nt' else 'c++')
 p.add_argument('--fetch-dependencies',action='store_true');p.add_argument('--plan',action='store_true')
 a=p.parse_args(argv)
 try:
  if platform.system() not in ('Windows','Linux'):raise ValueError('Use segarally_mac.py for macOS')
  out=output_path(a.output);rom=a.roms.expanduser().resolve()
  if not rom.is_file():raise ValueError('Personal ROM ZIP does not exist')
  plan=build_plan(rom,out,a.jobs,a.generation_compiler)
  if a.plan:print(json.dumps(plan,indent=2));return 0
  for tool in ('git','cmake','ctest',a.generation_compiler):
   if not shutil.which(tool):raise ValueError(f'Install prerequisite {tool} before building; no toolchain is installed automatically')
  dependencies(a.fetch_dependencies)
  out.mkdir(parents=True)
  revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
  report={'status':'building','source_revision':revision,'rom_sha256':digest(rom),'dependencies':DEPENDENCIES,'steps':[]}
  try:
   for i,(name,cmd,bound) in enumerate(plan):
    print(f'[{i+1}/{len(plan)}] {name}',flush=True)
    row={'name':name,'command':cmd,'status':'running'};report['steps'].append(row)
    run(cmd,out/f'{i+1:02d}-{name}.log',bound)
    if name=='sound':
     audit=json.loads((out/'native/sound/native-timed/audit.json').read_text())
     if not audit.get('native_scsp_startup_reached') or (audit['execution'].get('status')!='instruction_limit' or audit['execution'].get('instructions')!=1000000):
      raise ValueError('Sound startup did not finish its budget')
    row['status']='passed'
   report.update(status='packaged',package=package(out),launched=False,
     scope='Local build/package only; Windows/Linux Vulkan and gameplay remain unvalidated until tested on that platform')
  except BaseException as error:report.update(status='failed',error=str(error));raise
  finally:(out/'build-result.json').write_text(json.dumps(report,indent=2)+'\n')
  print(report['package']['archive']);return 0
 except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:print(f'Sega Rally desktop: {error}',file=sys.stderr);return 1
 except KeyboardInterrupt:return 130

if __name__=='__main__':raise SystemExit(main())
