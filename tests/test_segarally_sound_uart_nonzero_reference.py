#!/usr/bin/env python3
"""Purpose-corrected independent sound CPU gate; no production behavior changes.

Prepare authenticates all old failed-gate dependencies and the accepted public
purpose fixture, regenerates only pinned public reference microcode with labels,
and recompiles the comparator against the original measured native objects.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,re,shlex,sys,time
ROOT=Path(__file__).resolve().parents[1]
ORIGINAL_SHA='dee8607d0ef7793655f0d05c9a80d4d242ed3227718967dffa7d9b02832e07f3'
REFERENCE_SHA='9211911feedf343a30497f3ecca8c649865aec304471823296c25acd5e1f0ce4'
PURPOSE_SHA='9beb63e92096cee42aa8e0ea3617b6579a91f4670d2c4c475e5ef4d60ffabff7'
def require(value,message):
 if not value:raise RuntimeError(message)
def identity(path):
 require(path.is_file() and not path.is_symlink(),'regular frozen file: '+str(path));h=hashlib.sha256();size=0
 with path.open('rb') as stream:
  while block:=stream.read(65536):size+=len(block);require(size<=256*1024*1024,'bounded input');h.update(block)
 return {'bytes':size,'sha256':h.hexdigest()}
def save(path,value):
 with path.open('x') as stream:json.dump(value,stream,indent=2);stream.write('\n')
def load(path,digest):
 require(identity(path)['sha256']==digest,'pinned prerequisite: '+str(path));return json.loads(path.read_text())
def dependencies(original_dir,reference_dir,purpose_dir,sdk_settings):
 original=load(original_dir/'prepared.json',ORIGINAL_SHA);reference=load(reference_dir/'prepared.json',REFERENCE_SHA);proof=load(purpose_dir/'result.json',PURPOSE_SHA);require(proof['status']=='PASS','accepted public observer fixture')
 files={}
 def add(path,want=None):
  path=Path(path);actual=identity(path);require(want is None or actual==want,'unchanged dependency: '+str(path));files[str(path)]=actual
 for group in ['measured_inputs','artifacts']:
  for name,want in original[group].items():add(name,want)
 for name,want in proof['inputs'].items():add(name,want)
 for relative,want in proof['artifacts'].items():add(purpose_dir/relative,want)
 for path in [original_dir/'prepared.json',reference_dir/'prepared.json',purpose_dir/'result.json']:add(path)
 for name in ['test_m68k_timing_reference.py','m68k_reference_access_roles.py','test_m68k_reference_access_roles.cpp','test_m68k_reference_access_roles.py']:
  path=ROOT/'tests'/name;actual=identity(path);require(any(Path(old).name==name and want==actual for old,want in proof['inputs'].items()),'same passed purpose source: '+name);add(path)
 for name in ['model2a_sound_reference.h','test_segarally_sound_uart_nonzero.cpp']:
  path=ROOT/'tests'/name;actual=identity(path);require(any(Path(old).name==name and want==actual for old,want in original['measured_inputs'].items()),'unchanged actual provider/helper: '+name);add(path)
 for path in [Path(__file__).resolve(),ROOT/'tests/test_segarally_sound_uart_nonzero_reference.cpp']:add(path)
 require(sdk_settings.name=='SDKSettings.json','explicit compiler SDK metadata filename');add(sdk_settings)
 require(len(reference['authenticated_inventory'])==2663 and len({int(word,16) for pc,word in reference['authenticated_inventory']})==660,'authenticated full actual inventory')
 # Use the frozen watchdog implementation, whose own source is in files.
 candidates=[Path(p) for p in files if p.endswith('/tests/test_segarally_sound_uart_nonzero_reference.py') and Path(p)!=Path(__file__).resolve()]
 require(len(candidates)==1,'one measured original watchdog');spec=importlib.util.spec_from_file_location('frozen_nonzero_watchdog',candidates[0]);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 return original,reference,proof,files,module

def verify(manifest):
 for group in ['measured_inputs','artifacts']:
  for name,want in manifest[group].items():require(identity(Path(name))==want,'frozen closure changed: '+name)
def prepare(a):
 out=a.output;out.mkdir(parents=True,exist_ok=False);start=time.monotonic()
 try:
  original,reference,proof,before,watchdog=dependencies(a.original_prepared,a.reference_prepared,a.purpose_fixture,a.compiler_sdk_settings)
  reference_dir=a.reference_dir.resolve()
  for name in ['m68000.cpp','m68000.h','m68000.lst','m68000gen.py']:require(str(reference_dir/name) in before,'pinned MAME closure path: '+name)
  words=out/'opcodes.txt';words.write_text(''.join('%04x\n'%v for v in sorted({int(word,16) for pc,word in reference['authenticated_inventory']})))
  commands=[]
  for label,roles in [('plain',False),('oracle',True)]:
   command=[sys.executable,str(ROOT/'tests/test_m68k_timing_reference.py'),'--reference-dir',str(reference_dir),'--output',str(out/label),'--board-opcodes',str(words),'--board-opcode-limit','660']
   if roles:command.append('--board-access-roles')
   commands.append(command);watchdog.bounded(command,out,'generate-'+label,90)
  plain=(out/'plain/m68000-board-reference.h').read_bytes();expected=a.reference_prepared/'oracle/m68000-board-reference.h';require(str(expected) in before and plain==expected.read_bytes(),'default full660 generated CPU byte-identical to frozen provider')
  tagged=(out/'oracle/m68000-board-reference.h').read_text();stripped=re.sub(r"^[ \t]*observer_access_role = '[podi]';\n",'',tagged,flags=re.M).replace("char observer_access_role='?';\n ",'').replace("kind=='p'?observer_access_role:kind",'kind');require(stripped.encode()==plain,'all660 tagged CPU code reconstructs plain exactly')
  roles=json.loads((out/'oracle/m68000-access-roles.json').read_text());require(roles and any(r['role']=='o' and r['opcode']==0x337b for r in roles),'actual failing opcode has independently derived operand role')
  binary=out/'test-segarally-sound-uart-nonzero-reference';command=json.loads((a.original_prepared/'build-command.json').read_text());old_source=[x for x in command if x.endswith('/tests/test_segarally_sound_uart_nonzero_reference.cpp')];require(len(old_source)==1 and command[-1]==original['binary'] and command[-2]=='-o','original measured compiler command')
  command[command.index(old_source[0])]=str(ROOT/'tests/test_segarally_sound_uart_nonzero_reference.cpp');command[-1]=str(binary);command[7:7]=['-I'+str(out/'oracle')];command[1:1]=['-MMD','-MF',str(out/'include-dependencies.d')]
  commands.append(command);save(out/'commands.json',commands);seconds=watchdog.bounded(command,out,'build',90)
  require(before==dependencies(a.original_prepared,a.reference_prepared,a.purpose_fixture,a.compiler_sdk_settings)[3],'all old/new measured inputs unchanged')
  artifacts={str(p):identity(p) for p in out.rglob('*') if p.is_file()}
  # Non-system include closure from the actual successful compile, not just
  # declared capability flags. Original measured SHA/isystem inputs stay bound.
  deptext=(out/'include-dependencies.d').read_text().replace('\\\n',' ');include_paths=[Path(p).resolve() for p in shlex.split(deptext.split(':',1)[1])]
  require(a.compiler_sdk_settings in include_paths,'explicit compiler SDK metadata must be an actual include dependency')
  require(out/'oracle/m68000-board-reference.h' in include_paths,'actual compile consumes corrected purpose provider')
  unknown=[str(p) for p in include_paths if str(p) not in before and str(p) not in artifacts];require(not unknown,'unbound actual includes: '+repr(unknown))
  manifest={'status':'PREPARED_NOT_EXECUTED','binary':str(binary),'data':original['data'],'tape':original['tape'],'tape_sha256':original['tape_sha256'],'bounds':original['bounds'],'measured_inputs':before,'artifacts':artifacts,'compile_includes':list(map(str,include_paths)),'build_seconds':seconds,'original_manifest_sha256':ORIGINAL_SHA,'purpose_fixture_result_sha256':PURPOSE_SHA,'watchdog_source':str(Path(watchdog.__file__).resolve()),'scope':'Independent persistent MAME CPU/shared devices, strict full SR/state/data/PCM at original cuts. PROGRAM role o is ordered operand; role p is instruction stream. No physical bus-phase or whole sound-on recording claim.'}
  save(out/'prepared.json',manifest);print('PREPARED NOT EXECUTED:',out/'prepared.json')
 except BaseException as error:save(out/'preparation-failure.json',{'status':'FAIL','error':str(error),'seconds':time.monotonic()-start,'retry':'none'});raise

def execute(a):
 manifest_path=a.output/'prepared.json';m=json.loads(manifest_path.read_text());out=a.run_dir;out.mkdir(parents=True,exist_ok=False);start=time.monotonic()
 try:
  verify(m);spec=importlib.util.spec_from_file_location('frozen_watchdog',m['watchdog_source']);watchdog=importlib.util.module_from_spec(spec);spec.loader.exec_module(watchdog)
  command=['/usr/bin/nice','-n','10',m['binary'],m['data'],m['tape'],m['tape_sha256'],str(out/'state.json')];save(out/'command.json',{'argv':command,'prepared_manifest':identity(manifest_path),'bounds':m['bounds']});seconds=watchdog.bounded(command,out,'run',900);verify(m)
  state=json.loads((out/'state.json').read_text());require(state['status']=='PASS' and state['instructions']==14947325 and state['cycles']==186594177 and state['IRQ_entries']==14614 and state['original_cuts']==1929 and state['PCM_frames']==728884 and state['PCM']['nonzero_scalar_samples']==22784,'unchanged exact first-nonzero terminal')
  save(out/'result.json',{'status':'PASS','seconds':seconds,'total_seconds':time.monotonic()-start,'inputs_unchanged':True,'prepared_manifest':identity(manifest_path),'state':state,'artifacts':{str(p.relative_to(out)):identity(p) for p in out.rglob('*') if p.is_file()}});print('PASS:',out/'result.json')
 except BaseException as error:
  closure_error=None
  try:verify(m)
  except BaseException as issue:closure_error=str(issue)
  save(out/'result.json',{'status':'FAIL','error':str(error),'seconds':time.monotonic()-start,'inputs_unchanged':closure_error is None,'closure_error':closure_error,'retry':'none','artifacts':{str(p.relative_to(out)):identity(p) for p in out.rglob('*') if p.is_file()}});raise

def main():
 p=argparse.ArgumentParser(description=__doc__);mode=p.add_mutually_exclusive_group(required=True);mode.add_argument('--prepare',action='store_true');mode.add_argument('--run',action='store_true');p.add_argument('--output',type=Path,required=True)
 for option in ['original-prepared','reference-prepared','purpose-fixture','reference-dir','compiler-sdk-settings','run-dir']:p.add_argument('--'+option,type=Path)
 a=p.parse_args()
 for name,value in vars(a).items():
  if isinstance(value,Path):setattr(a,name,value.resolve())
 require(a.output.is_relative_to(ROOT/'build'),'exclusive ignored output');deps=[a.original_prepared,a.reference_prepared,a.purpose_fixture,a.reference_dir,a.compiler_sdk_settings]
 if a.prepare:require(all(deps) and a.run_dir is None,'prepare dependencies only');prepare(a)
 else:require(not any(deps) and a.run_dir and a.run_dir.is_relative_to(ROOT/'build'),'run exclusive frozen prepared binary only');execute(a)
if __name__=='__main__':main()
