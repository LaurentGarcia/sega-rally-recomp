#!/usr/bin/env python3
"""Prepare, capture, or natively recapture real Rally UART transactions.

Source-only preparation is reviewable without running this script. --prepare
compiles only after clearance; capture and recapture are separate bounded modes.
No independent CPU, shipped interpreter, GUI, device output, ROM generation or
production identity bypass. Existing measured host objects remain immutable.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,shlex,signal,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
MAX_LOG=16*1024*1024

def require(value,message):
 if not value:raise RuntimeError(message)
def identity(path):
 require(path.is_file() and not path.is_symlink(),f'regular file required: {path}')
 digest=hashlib.sha256();size=0
 with path.open('rb') as stream:
  while block:=stream.read(65536):size+=len(block);require(size<=256*1024*1024,'bounded input');digest.update(block)
 return {'bytes':size,'sha256':digest.hexdigest()}
def save(path,data):
 with path.open('x') as stream:json.dump(data,stream,indent=2);stream.write('\n')
def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result

def authenticate(args):
 helper=module(ROOT/'tests/test_segarally_recording_reference.py','uart_host_binding')
 host_root,sources,files=helper.verify_host(args.host_build,args.host_evidence)
 initial=module(ROOT/'tests/test_segarally_sound_init_reference.py','uart_sound_binding')
 # This reuses actual image/native-generation audit pins, all guarded ROM
 # words, and full runtime closure. MAME files are identity-only here: no
 # reference CPU is compiled or executed by this native recapture gate.
 sound_inputs=initial.source_closure(args.generation/'sound',args.reference_dir)
 require(len(initial.inventory(args.generation/'sound'))==2663,'actual sound instruction inventory')
 files.update(map(Path,sound_inputs))
 sys.path.insert(0,str(host_root/'scripts'))
 audit=module(host_root/'scripts/segarally_recompile.py','uart_main_audit')
 coverage=helper.read_json(args.generation/'coverage.json')
 boundary=audit.verify_boundary_contract(args.generation,coverage,verify_objects=True)
 require(boundary is not None,'actual boundary main generation required')
 stage=module(host_root/'platform/segarally/stage_data.py','uart_staging')
 assets_dir=args.host_build/'assets';assets=helper.read_json(assets_dir/'assets.json')
 audits=[coverage,helper.read_json(args.generation/'tgp/audit.json'),helper.read_json(args.generation/'video/audit.json')]
 require(assets['romset']=='srallycb' and assets['provenance']==stage.staging_provenance(args.generation,audits,boundary),'actual staged provenance')
 require(set(assets['images'])=={'program','main_data','tgp_tables','copro_data','polygons','textures','sound_program','sound_samples'},'all8 actual images')
 for name,item in assets['images'].items():
  require(item['file']==name+'.bin','canonical asset name');path=assets_dir/item['file']
  require(identity(path)=={'bytes':item['size'],'sha256':item['sha256']},'actual asset binding');files.add(path)
 for name,digest in boundary['generated_sha256'].items():
  path=args.generation/'gen'/name;require(path in sources and identity(path)['sha256']==digest,'main generated source/object binding');files.add(path)
 for relative in ['coverage.json','reachability.json','tgp/audit.json','video/audit.json','sound/audit.json','sound/native-timed/audit.json']:
  files.add(args.generation/relative)
 files.add(assets_dir/'assets.json')
 # Use actual compiled native target inputs, omitting presentation and the
 # original sound generated object (its unchanged body is locally observed).
 objects=[]
 for source,obj in sources.items():
  if 'segarally_native.dir' in str(obj) and '/src/app/' not in str(source) and source!=args.generation/'sound/native-timed/snd_gen.cpp':
   objects.append(obj)
   if '/src/runtime/' in str(source):require(identity(source)==identity(ROOT/source.relative_to(host_root)),'native runtime mismatch')
 crypto=[];sha_dir=None
 for suffix in ['/src/runtime/sha256_digest.cpp','/C/Sha256.c','/C/Sha256Opt.c','/C/CpuArch.c']:
  matches=[(s,o) for s,o in sources.items() if str(s).endswith(suffix) and 'segarally_recording.dir' in str(o)]
  require(len(matches)==1,'exact SHA object');s,o=matches[0];crypto.append(o)
  if suffix=='/C/Sha256.c':sha_dir=s.parent
 require(objects and sha_dir is not None,'native objects and pinned SHA required')
 revision=subprocess.run(['git','-C',str(sha_dir.parent),'rev-parse','HEAD'],check=True,capture_output=True,text=True).stdout.strip()
 require(revision=='0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17','pinned SHA dependency revision')
 subprocess.run(['git','-C',str(sha_dir.parent),'diff','--quiet','HEAD','--','C'],check=True)
 files.update(objects+crypto);files.update(sha_dir.glob('*.h'))
 # Include the complete actual current runtime header closure used by the
 # new translation unit, and both reused verifiers, in the reviewed manifest.
 for path in (ROOT/'src/runtime').iterdir():
  if path.is_file():
   old=host_root/'src/runtime'/path.name;require(identity(path)==identity(old),'measured runtime/header byte identity');files.update((path,old))
 files.update(ROOT/'tests'/name for name in ['test_segarally_recording_reference.py','test_segarally_sound_init_reference.py','test_segarally_sound_uart_capture.cpp','test_segarally_sound_uart_capture.py'])
 return assets_dir,assets,objects+crypto,sha_dir,{str(p):identity(p) for p in sorted(files)}

def prepare(args):
 out=args.output;require(out.is_relative_to(ROOT/'build'),'ignored exclusive preparation directory required');out.mkdir(parents=True,exist_ok=False)
 data,assets,objects,sha_dir,before=authenticate(args)
 native=args.generation/'sound/native-timed/snd_gen.cpp'
 (out/'segarally-sound-reset-inputs.h').write_text('#pragma once\n#define SOUND_REFERENCE_NATIVE_SOURCE '+json.dumps(str(native))+'\n')
 report=identity(data/'assets.json')['sha256']
 text='#pragma once\n#include <array>\n#include <string_view>\n#include <cstdint>\nnamespace uart_inputs {struct Asset {std::string_view file;uint64_t size;std::string_view sha256;};\ninline constexpr std::array<Asset,8> assets{{\n'
 text+=''.join('{'+json.dumps(v['file'])+','+str(v['size'])+','+json.dumps(v['sha256'])+'},\n' for k,v in sorted(assets['images'].items()))
 text+='}};\ninline constexpr std::string_view assets_report_sha256="'+report+'";\n}\n';(out/'segarally-uart-build-inputs.h').write_text(text)
 binary=out/'test-segarally-sound-uart-capture'
 command=['/usr/bin/c++','-std=c++20','-O1','-g','-fno-fast-math','-ffp-contract=off','-DM2_INSTRUCTION_BOUNDARY_HOOK=1','-I'+str(ROOT/'src'),'-I'+str(out),'-isystem',str(sha_dir),str(ROOT/'tests/test_segarally_sound_uart_capture.cpp'),*map(str,objects),'-o',str(binary)]
 save(out/'build-command.json',command)
 with (out/'build.stdout').open('xb') as so,(out/'build.stderr').open('xb') as se:subprocess.run(command,cwd=ROOT,stdout=so,stderr=se,check=True,timeout=90)
 require(before==authenticate(args)[-1],'unchanged native source/object/assets closure')
 prepared={'status':'PREPARED_NOT_EXECUTED','binary':str(binary),'data':str(data),'measured_inputs':before,
  'artifacts':{str(p):identity(p) for p in out.iterdir() if p.is_file()},
  'bounds':{'main_instructions':20000000,'board_ns':2000000000,'wall_seconds':90,'UART_rows':4096,'tape_bytes':MAX_LOG,'log_bytes':MAX_LOG,'artifact_bytes':32*1024*1024},
  'scope':'Authentic native Machine UART operations then separate native sound recapture. Independent reference CPU and sound-on recording capability remain unsupported. Shared instruction-start device models; original source/object evidence is measured binding, not hardware proof.'}
 save(out/'prepared.json',prepared);print('PREPARED NOT EXECUTED:',out/'prepared.json')

def execute(args):
 prepared_path=args.output/'prepared.json';prepared=json.loads(prepared_path.read_text());out=args.run_dir
 require(out.is_relative_to(ROOT/'build'),'ignored exclusive execution directory required');out.mkdir(parents=True,exist_ok=False)
 process=None;start=time.monotonic()
 try:
  def pins():
   for group in ['measured_inputs','artifacts']:
    for name,item in prepared[group].items():require(identity(Path(name))==item,'frozen input changed: '+name)
  pins();tape=None;tape_identity=None
  if args.capture:
   command=[prepared['binary'],'capture',prepared['data'],str(out/'capture')]
  else:
   capture=args.capture_dir;result=json.loads((capture/'result.json').read_text())
   require(result['status']=='PASS' and result['mode']=='capture' and result['prepared_manifest']==identity(prepared_path),'successful capture from exact same prepared binary required')
   tape=capture/'capture/uart.txt';tape_identity=identity(tape);require(tape_identity==result['artifacts']['capture/uart.txt'],'immutable complete capture tape')
   for name in ['initial.eeprom.bin','initial.backup.bin']:
    path=capture/'capture'/name;require(identity(path)==result['artifacts']['capture/'+name] and all(v==255 for v in path.read_bytes()),'actual fresh captured seeds')
   command=[prepared['binary'],'recapture',prepared['data'],str(tape),tape_identity['sha256']]
  save(out/'command.json',{'argv':command,'prepared_manifest':identity(prepared_path),'bounds':prepared['bounds']})
  with (out/'stdout.txt').open('xb') as so,(out/'stderr.txt').open('xb') as se:
   process=subprocess.Popen(command,cwd=ROOT,stdout=so,stderr=se,start_new_session=True)
   while process.poll() is None:
    require(time.monotonic()-start<=90,'90s process-group watchdog')
    require(sum((out/n).stat().st_size for n in ['stdout.txt','stderr.txt'])<=MAX_LOG,'16MiB log cap')
    require(sum(p.stat().st_size for p in out.rglob('*') if p.is_file())<=32*1024*1024,'32MiB output cap');time.sleep(.2)
  require(process.returncode==0,'first native gate failure: exit '+str(process.returncode));pins()
  if tape is not None:require(identity(tape)==tape_identity,'captured tape unchanged after recapture')
  require(('PASS authentic UART capture;' if args.capture else 'PASS native UART recapture accesses=') in (out/'stdout.txt').read_text(),'normal scoped terminal marker')
  save(out/'result.json',{'status':'PASS','mode':'capture' if args.capture else 'recapture','seconds':time.monotonic()-start,'prepared_manifest':identity(prepared_path),'inputs_unchanged':True,'artifacts':{str(p.relative_to(out)):identity(p) for p in out.rglob('*') if p.is_file()}})
  print('PASS:',out/'result.json')
 except BaseException as error:
  if process is not None and process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=3)
  save(out/'result.json',{'status':'FAIL','error':str(error),'seconds':time.monotonic()-start,'exit_code':None if process is None else process.returncode,'retry':'none'});raise

def main():
 p=argparse.ArgumentParser(description=__doc__);m=p.add_mutually_exclusive_group(required=True)
 for name in ['prepare','capture','recapture']:m.add_argument('--'+name,action='store_true')
 p.add_argument('--output',type=Path,required=True)
 for name in ['generation','host-build','host-evidence','reference-dir','run-dir','capture-dir']:p.add_argument('--'+name,type=Path)
 args=p.parse_args()
 for name,value in vars(args).items():
  if isinstance(value,Path):setattr(args,name,value.resolve())
 if args.prepare:require(all([args.generation,args.host_build,args.host_evidence,args.reference_dir]) and not(args.run_dir or args.capture_dir),'prepare uses measured build paths only');prepare(args)
 else:require(args.run_dir and not any([args.generation,args.host_build,args.host_evidence,args.reference_dir]) and bool(args.capture_dir)==args.recapture,'execution uses frozen prepared identity and exclusive run directory');execute(args)
if __name__=='__main__':main()
