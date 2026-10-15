#!/usr/bin/env python3
"""Manual/CI ROM-free native Vulkan CRT build/run. No SDL, game or window."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import signal
import sys
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,required=True);p.add_argument('--require-software',action='store_true')
 a=p.parse_args();out=a.output.resolve()
 if out.exists() or out==ROOT/'build' or not out.is_relative_to(ROOT/'build'):p.error('output must be a NEW child of ignored build/')
 inputs=['platform/segarally/native-vulkan/CMakeLists.txt','platform/segarally/native-vulkan/crt_smoke.cpp',
         'src/app/gpu/crt.hlsl','src/app/gpu/crt_shaders_gen.h','tests/test_native_vulkan_crt.py']
 before={name:sha(ROOT/name) for name in inputs};out.mkdir(parents=True)
 result={'status':'running','inputs':before,'scope':'Native Vulkan offscreen common CRT shader on invented pixels; no SDL/ROM/game/surface/audio or performance claim','commands':[]}
 try:
  def run(cmd,label,timeout):
   result['commands'].append(cmd)
   with (out/(label+'.stdout')).open('wb') as stdout,(out/(label+'.stderr')).open('wb') as stderr:
    kwargs={'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=='nt' else {'start_new_session':True}
    child=subprocess.Popen(cmd,cwd=ROOT,stdout=stdout,stderr=stderr,**kwargs)
    try:
     code=child.wait(timeout=timeout)
     if code:raise subprocess.CalledProcessError(code,cmd)
    except BaseException:
     if child.poll() is None:
      if os.name=='nt':
       subprocess.run(['taskkill','/PID',str(child.pid),'/T','/F'],capture_output=True,timeout=10)
       child.wait(timeout=10)
      else:
       try:os.killpg(child.pid,signal.SIGTERM)
       except ProcessLookupError:pass
       try:child.wait(timeout=5)
       except subprocess.TimeoutExpired:
        os.killpg(child.pid,signal.SIGKILL);child.wait()
     raise
  run(['cmake','-S',str(ROOT/'platform/segarally/native-vulkan'),'-B',str(out/'compile'),'-DCMAKE_BUILD_TYPE=Release'],'configure',120)
  run(['cmake','--build',str(out/'compile'),'--config','Release','--parallel','2'],'build',180)
  name='test_segarally_native_vulkan'+('.exe' if os.name=='nt' else '')
  candidates=[exe for exe in (out/'compile'/name,out/'compile/Release'/name) if exe.is_file()]
  if len(candidates)!=1:raise RuntimeError('Cannot identify unique compiled fixture')
  exe=candidates[0];result['executable_sha256']=sha(exe)
  run([str(exe),'--output',str(out/'render'),*(['--require-software'] if a.require_software else [])],'run',30)
  # Independent scalar pass over the persisted actual Vulkan readback, with
  # one UNORM unit tolerance for backend rounding. Every RGBA byte is checked.
  expected=bytes(value for y in range(48) for x in range(64) for value in (20+x*3,40+y*4,100,255))
  assert (out/'render/input.rgba').read_bytes()==expected
  checks=0
  for passno,name in enumerate(('identity.rgba','mask.rgba')):
   raw=(out/'render'/name).read_bytes();assert len(raw)==64*48*4
   for i,(actual,original) in enumerate(zip(raw,expected)):
    channel=i%4;x=(i//4)%64
    value=original/2 if passno and channel<3 and channel!=x%3 else original
    assert abs(actual-value)<=1,(name,i,actual,value)
    checks+=1
  assert '2 draws/readbacks' in (out/'run.stdout').read_text()
  assert before=={name:sha(ROOT/name) for name in inputs},'Sources changed during gate'
  result.update(status='passed',independent_channel_checks=checks,
    raw_images={p.name:{'size':p.stat().st_size,'sha256':sha(p)} for p in sorted((out/'render').iterdir())})
 except BaseException as error:
  result.update(status='failed',error=str(error));raise
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(out/'result.json')
if __name__=='__main__':main()
