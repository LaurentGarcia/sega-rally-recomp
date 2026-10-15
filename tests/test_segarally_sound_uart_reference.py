#!/usr/bin/env python3
"""Prepare/run independent sound CPU on the frozen authentic UART prefix.

Reuses authenticated native recapture and full660-opcode MAME preparation.
Original cuts and artifacts stay immutable; each gate gets exclusive outputs.
"""
from pathlib import Path
import argparse,hashlib,json,os,signal,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
PINS={
 'native_manifest':'c7e38037c47d3b6cb4a47c9f7a0da6f7bd1d04563138d0f08d532f373436f651',
 'reference_manifest':'9211911feedf343a30497f3ecca8c649865aec304471823296c25acd5e1f0ce4',
 'capture_result':'ebe3f63303ba5eafacbba12ecd9cbdc4a4c5624f8d493e78524e001cf7a49c12',
 'native_recapture_result':'fc7fbb3bfb76225c72cfd573add35a990ec2ffbc30a35a3f7fcb63657a210022',
 'tape':'215804ec9b993026eeec668b36127c27522411d58bb0d89f486c060006dbabd2'}
def require(value,message):
 if not value:raise RuntimeError(message)
def identity(path):
 require(path.is_file() and not path.is_symlink(),'regular file required: '+str(path));digest=hashlib.sha256();count=0
 with path.open('rb') as stream:
  while block:=stream.read(65536):count+=len(block);require(count<=256*1024*1024,'bounded input');digest.update(block)
 return {'bytes':count,'sha256':digest.hexdigest()}
def save(path,value):
 with path.open('x') as stream:json.dump(value,stream,indent=2);stream.write('\n')
def load_pinned(path,label):
 require(identity(path)['sha256']==PINS[label],'frozen '+label+' changed');return json.loads(path.read_text())
def bind(native_dir,reference_dir,capture_dir,recapture_dir):
 native=load_pinned(native_dir/'prepared.json','native_manifest');reference=load_pinned(reference_dir/'prepared.json','reference_manifest')
 captured=load_pinned(capture_dir/'result.json','capture_result');recaptured=load_pinned(recapture_dir/'result.json','native_recapture_result')
 require(captured['status']==recaptured['status']=='PASS','native capture/recapture successful prerequisites')
 tape=capture_dir/'capture/uart.txt';require(identity(tape)['sha256']==PINS['tape'],'authentic unchanged tape')
 require(len(reference['authenticated_inventory'])==2663 and len({int(word,16) for pc,word in reference['authenticated_inventory']})==660,'full audited PC/opcode profile')
 files={}
 for manifest,groups in [(native,['measured_inputs','artifacts']),(reference,['source_inputs','prepared_artifacts'])]:
  for group in groups:
   for name,item in manifest[group].items():
    require(identity(Path(name))==item,'unchanged prepared dependency: '+name)
    if name in files:require(files[name]==item,'overlapping binding disagreement')
    files[name]=item
 for directory in [capture_dir,recapture_dir]:
  result=json.loads((directory/'result.json').read_text())
  for relative,item in result['artifacts'].items():
   path=directory/relative;require(identity(path)==item,'native evidence artifact changed');files[str(path)]=item
 for path in [native_dir/'prepared.json',reference_dir/'prepared.json',capture_dir/'result.json',recapture_dir/'result.json',tape,Path(__file__).resolve(),ROOT/'tests/test_segarally_sound_uart_reference.cpp']:
  files[str(path)]=identity(path)
 return native,reference,files

def bounded(command,out,label,seconds):
 start=time.monotonic();process=None
 try:
  with (out/(label+'.stdout')).open('xb') as so,(out/(label+'.stderr')).open('xb') as se:
   process=subprocess.Popen(command,cwd=ROOT,stdout=so,stderr=se,start_new_session=True)
   while process.poll() is None:
    require(time.monotonic()-start<=seconds,str(seconds)+'s process-group watchdog')
    require(sum((out/(label+'.'+suffix)).stat().st_size for suffix in ['stdout','stderr'])<=16*1024*1024,'16MiB logs')
    # A new33MiB executable is allowed during compilation; run artifacts cap32MiB.
    require(sum(p.stat().st_size for p in out.rglob('*') if p.is_file())<=(128 if label=='build' else 32)*1024*1024,'bounded artifacts');time.sleep(.2)
  require(process.returncode==0,'first '+label+' failure: exit '+str(process.returncode))
  return time.monotonic()-start
 except BaseException:
  if process is not None and process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=3)
  raise

def prepare(args):
 out=args.output;require(out.is_relative_to(ROOT/'build'),'ignored exclusive preparation');out.mkdir(parents=True,exist_ok=False);start=time.monotonic()
 try:
  native,reference,before=bind(args.native_prepared,args.reference_prepared,args.capture_dir,args.native_recapture_dir)
  old=json.loads((args.native_prepared/'build-command.json').read_text());require(old[0]=='/usr/bin/c++','reviewed compiler')
  objects=[Path(token) for token in old if token.endswith('.o')];require(objects and all(str(p) in before for p in objects),'measured original native objects only')
  require(old.count('-isystem')==1,'pinned SHA include');sha_dir=Path(old[old.index('-isystem')+1]);require(str(sha_dir/'Sha256.h') in before,'SHA header binding')
  binary=out/'test-segarally-sound-uart-reference'
  command=['/usr/bin/c++','-std=c++20','-O1','-g','-fno-fast-math','-ffp-contract=off','-DM2_INSTRUCTION_BOUNDARY_HOOK=1','-I'+str(ROOT/'src'),'-I'+str(args.native_prepared),'-I'+str(args.reference_prepared),'-I'+str(args.reference_prepared/'oracle'),'-isystem',str(sha_dir),str(ROOT/'tests/test_segarally_sound_uart_reference.cpp'),*map(str,objects),'-o',str(binary)]
  save(out/'build-command.json',command);seconds=bounded(command,out,'build',90)
  require(before==bind(args.native_prepared,args.reference_prepared,args.capture_dir,args.native_recapture_dir)[2],'build inputs unchanged')
  prepared={'status':'PREPARED_NOT_EXECUTED','binary':str(binary),'data':native['data'],'tape':str(args.capture_dir/'capture/uart.txt'),'tape_sha256':PINS['tape'],'measured_inputs':before,'artifacts':{str(p):identity(p) for p in out.iterdir() if p.is_file()},'build_seconds':seconds,
   'bounds':{'sound_instructions':1260947,'board_ns':2000000000,'paired_observations':4000000,'wall_seconds':90,'logs_bytes':16777216,'output_bytes':33554432},
   'scope':'Independent persistent MAME CPU on authentic UART traffic; exact old native cuts, separate shared board models, instruction-start MMIO. Reference phase evidence separate; silent PCM, no titlemusic/physicalphase/whole-recording claim.'}
  save(out/'prepared.json',prepared);print('PREPARED NOT EXECUTED:',out/'prepared.json')
 except BaseException as error:save(out/'preparation-failure.json',{'status':'FAIL','error':str(error),'seconds':time.monotonic()-start,'retry':'none'});raise

def execute(args):
 prepared_path=args.output/'prepared.json';prepared=json.loads(prepared_path.read_text());out=args.run_dir;require(out.is_relative_to(ROOT/'build'),'ignored exclusive execution');out.mkdir(parents=True,exist_ok=False);start=time.monotonic()
 try:
  def pins():
   for group in ['measured_inputs','artifacts']:
    for name,item in prepared[group].items():require(identity(Path(name))==item,'frozen input changed: '+name)
  pins();command=[prepared['binary'],prepared['data'],prepared['tape'],prepared['tape_sha256'],str(out/'state.json')];save(out/'command.json',{'argv':command,'prepared_manifest':identity(prepared_path),'bounds':prepared['bounds']})
  seconds=bounded(command,out,'run',90);pins();state=json.loads((out/'state.json').read_text())
  require(state['status']=='PASS' and state['instructions']==1260947 and state['cycles']==16312806 and state['IRQ_entries']==660 and state['original_cuts']==163 and state['PCM_frames']==63722,'exact authentic independent terminal')
  save(out/'result.json',{'status':'PASS','seconds':seconds,'total_seconds':time.monotonic()-start,'prepared_manifest':identity(prepared_path),'inputs_unchanged':True,'state':state,'artifacts':{str(p.relative_to(out)):identity(p) for p in out.rglob('*') if p.is_file()}});print('PASS:',out/'result.json')
 except BaseException as error:save(out/'result.json',{'status':'FAIL','error':str(error),'seconds':time.monotonic()-start,'retry':'none'});raise

def main():
 p=argparse.ArgumentParser(description=__doc__);m=p.add_mutually_exclusive_group(required=True);m.add_argument('--prepare',action='store_true');m.add_argument('--run',action='store_true');p.add_argument('--output',type=Path,required=True)
 for name in ['native-prepared','reference-prepared','capture-dir','native-recapture-dir','run-dir']:p.add_argument('--'+name,type=Path)
 args=p.parse_args()
 for name,value in vars(args).items():
  if isinstance(value,Path):setattr(args,name,value.resolve())
 deps=[args.native_prepared,args.reference_prepared,args.capture_dir,args.native_recapture_dir]
 if args.prepare:require(all(deps) and not args.run_dir,'prepare frozen prerequisite paths');prepare(args)
 else:require(args.run_dir and not any(deps),'run frozen prepared binary only');execute(args)
if __name__=='__main__':main()
