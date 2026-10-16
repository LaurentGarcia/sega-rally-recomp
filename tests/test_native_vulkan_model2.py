#!/usr/bin/env python3
"""Compare direct/SDL Vulkan for invented and optional private recorded scenes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',required=True,type=Path);p.add_argument('--sdl-source',type=Path,default=ROOT/'extern/sdl3')
 p.add_argument('--require-software',action='store_true')
 p.add_argument('--require-validation',action='store_true')
 p.add_argument('--build-dir',type=Path,help='Optional ignored build cache to reuse')
 p.add_argument('--scene',type=Path,action='append',default=[],help='Private recorded scene directory; never uploaded')
 a=p.parse_args();out=a.output.resolve();sdl=a.sdl_source.resolve()
 compile_dir=a.build_dir.resolve() if a.build_dir else out/'compile'
 if not compile_dir.is_relative_to(ROOT/'build') or compile_dir==ROOT/'build':p.error('build-dir must be below ignored build/')
 scenes=[path.resolve() for path in a.scene]
 if any(not path.is_dir() for path in scenes):p.error('scene must name an existing directory')
 if out.exists() or out==ROOT/'build' or not out.is_relative_to(ROOT/'build'):p.error('output must be a NEW directory below ignored build/')
 revision=subprocess.check_output(['git','-C',str(sdl),'rev-parse','HEAD'],text=True).strip()
 assert revision=='fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'
 changes=subprocess.check_output(['git','-C',str(sdl),'diff','HEAD','--'])
 if changes:p.error('SDL reference must have no tracked modifications')
 measured=['src/app/native/vulkan/model2_vulkan.h','src/app/native/vulkan/model2_vulkan.cpp',
 'src/app/gpu/model2_frame.h','src/app/gpu/model2_frame.cpp','src/app/gpu/model2_resources.h',
 'src/app/gpu/shaders_gen.h','src/app/gpu/m2.hlsl','src/app/gpu/normal_reciprocal.hlsl',
 'src/app/gpu/gpu_renderer.h','src/app/gpu/gpu_renderer.cpp','src/app/gpu_device.h',
 'src/runtime/video.h','src/runtime/video.cpp','src/runtime/raster.h','src/runtime/raster.cpp',
 'src/runtime/model2_priority.h','tests/model2_recorded_fixture.h','tests/model2_native_fixture.h','tests/test_native_vulkan_model2.cpp',
 'tests/test_native_vulkan_model2.py','tests/test_native_vulkan_async.cpp','platform/segarally/native-vulkan-model2/CMakeLists.txt']
 before={name:sha(ROOT/name) for name in measured};out.mkdir(parents=True)
 result={'status':'running','inputs':before,'sdl_revision':revision,'sdl_diff_sha256':hashlib.sha256(changes).hexdigest(),
 'scope':'Native Vulkan consumes common Model2Frame/resources; invented/optional recorded comparisons against separate pinned SDL Vulkan binary. No SDL in native linkage, game execution/audio/OS surface or performance evidence.','validation_required':a.require_validation,'commands':[]}
 private_inputs={str(path):{p.name:sha(p) for p in sorted(path.iterdir()) if p.is_file()} for path in scenes}
 result['private_inputs']=private_inputs
 env=dict(os.environ)
 for key in list(env):
  if key.startswith(('SDL_','SEGARALLY_')):env.pop(key)
 env['SDL_GPU_DRIVER']='vulkan'
 if a.require_validation:env['VK_INSTANCE_LAYERS']='VK_LAYER_KHRONOS_validation'
 if sys.platform.startswith('linux'):env['SDL_VIDEODRIVER']='offscreen'
 def run(cmd,label,timeout):
  result['commands'].append(cmd)
  with (out/(label+'.stdout')).open('wb') as stdout,(out/(label+'.stderr')).open('wb') as stderr:
   kwargs={'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=='nt' else {'start_new_session':True}
   child=subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=stdout,stderr=stderr,**kwargs)
   try:
    code=child.wait(timeout=timeout)
    if code:raise subprocess.CalledProcessError(code,cmd)
   except BaseException:
    if child.poll() is None:
     if os.name=='nt':
      subprocess.run(['taskkill','/PID',str(child.pid),'/T','/F'],capture_output=True,timeout=10);child.wait(timeout=10)
     else:
      try:os.killpg(child.pid,signal.SIGTERM)
      except ProcessLookupError:pass
      try:child.wait(timeout=5)
      except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait()
    raise
 try:
  run(['cmake','-S',str(ROOT/'platform/segarally/native-vulkan-model2'),'-B',str(compile_dir),'-DCMAKE_BUILD_TYPE=Release',
       '-DSEGARALLY_VULKAN_SDL_REFERENCE=ON',f'-DSEGARALLY_SDL_SOURCE={sdl}'],'configure',600)
  run(['cmake','--build',str(compile_dir),'--config','Release','--parallel','2'],'build',600)
  def binary(name):
   name+=('.exe' if os.name=='nt' else '')
   paths=[p for p in (compile_dir/name,compile_dir/'Release'/name) if p.is_file()]
   assert len(paths)==1
   return paths[0]
  native=binary('test_native_vulkan_model2');reference=binary('test_sdl_vulkan_model2')
  async_binary=binary('test_native_vulkan_async')
  result['executables']={'native':sha(native),'reference':sha(reference),'async':sha(async_binary)}
  for name in ('reference','native'):(out/name).mkdir()
  scene_args=[v for scene in scenes for v in ('--scene',str(scene))]
  run([str(reference),str(out/'reference'),*scene_args],'reference',180)
  run([str(native),str(out/'native'),*(['--require-software'] if a.require_software else []),*(['--require-validation'] if a.require_validation else []),*scene_args],'native',180)
  run([str(async_binary),*(['--require-software'] if a.require_software else []),*(['--require-validation'] if a.require_validation else [])],'async',60)
  result['images']=[]
  for i in range(26+4*len(scenes)):
   name=f'scene-{i:02d}.bgra';expected=(out/'reference'/name).read_bytes();actual=(out/'native'/name).read_bytes()
   scale=2 if (i<24 or i>=26) and i%2 else 1;width=560 if i==24 else 496
   assert len(actual)==len(expected)==width*384*scale*scale*4
   differences=sum(actual[n:n+4]!=expected[n:n+4] for n in range(0,len(actual),4))
   result['images'].append({'name':name,'pixels':len(actual)//4,'differences':differences,'native_sha256':sha(out/'native'/name),'reference_sha256':sha(out/'reference'/name)})
   assert actual==expected,(name,differences)
  assert before=={name:sha(ROOT/name) for name in measured}
  assert subprocess.check_output(['git','-C',str(sdl),'diff','HEAD','--'])==changes
  assert private_inputs=={str(path):{p.name:sha(p) for p in sorted(path.iterdir()) if p.is_file()} for path in scenes}
  if a.require_validation:
   for name in ('native','reference','async'):
    logs=(out/(name+'.stdout')).read_text()+(out/(name+'.stderr')).read_text()
    assert 'Validation Error' not in logs and 'VUID-' not in logs,(name,'validation diagnostics')
  if sys.platform.startswith('linux'):
   linkage=subprocess.check_output(['ldd',str(native)],text=True);(out/'native-linkage.txt').write_text(linkage)
   assert 'SDL' not in linkage
  result['status']='passed'
 except BaseException as error:result.update(status='failed',error=str(error));raise
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(out/'result.json')
if __name__=='__main__':main()
