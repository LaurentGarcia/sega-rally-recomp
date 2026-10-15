#!/usr/bin/env python3
"""Prepare or explicitly run the authenticated Rally sound initialization checkpoint.

CPU-only, no app build/output device; existing user-provided images stay intact.
Preparation compiles but NEVER executes the real program. Sound-on recording
validation remains unsupported. A separate --run executes one bounded gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,signal,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
PINS={'audit.json':'5729d3019fd045669befa434af2ca8cd137c1df8648b119cf90be4b3a5c87143',
 'audiocpu.bin':'6be2a7a6c9c08b55291ceaf7891f2d8418e38291ed00376cf0f569a8a083ad6f',
 'samples.bin':'4a561c46de5400040ea21322040fbfef1c91e8dc4e9fc7a5123490187b7896ba',
 'native-timed/audit.json':'b7d1f0dcb030a08e2c474fb674f79fc729ce047e90a697ff86fa3a2dc08fb032',
 'native-timed/generation.json':'92de3e03bc9921178993d44807bffcce87d190ce76eaa1472cfc4c023a6bd758',
 'native-timed/seeds.txt':'032e9408ff78679898401c785b79ce76a74a4d52f9cbbb272a269cc77cbf7158',
 'native-timed/snd_gen.cpp':'011c0e18fbf23c3c36f8ca7154e1389c06fac4adfbfd0e1f82456997af21005b'}
def require(value,message):
 if not value:raise RuntimeError(message)
def identity(path):
 require(path.is_file() and not path.is_symlink(),'regular input required: '+str(path))
 digest=hashlib.sha256()
 with path.open('rb') as f:
  while chunk:=f.read(65536):digest.update(chunk)
 return {'bytes':path.stat().st_size,'sha256':digest.hexdigest()}
def save(path,value):
 with path.open('x') as f:json.dump(value,f,indent=2);f.write('\n')
def source_closure(sound,refs):
 result={}
 for name,digest in PINS.items():
  path=sound/name;item=identity(path);require(item['sha256']==digest,'preserved sound pin: '+name);result[str(path)]=item
 audit=json.loads((sound/'native-timed/audit.json').read_text())
 require(audit['generated_sha256']==PINS['native-timed/snd_gen.cpp'],'native source/audit binding')
 require(audit['generation']['instructions']==2663 and audit['generation']['missing_templates']==audit['generation']['missing_timing']==0,'complete audited native inventory')
 for name,digest in audit['sources_sha256'].items():
  path=ROOT/name;item=identity(path);require(item['sha256']==digest,'actual generation source differs: '+name);result[str(path)]=item
 # Include all transitive project runtime headers/sources rather than guessing
 # which dependencies the unchanged generated native source might include.
 for directory in [ROOT/'src/runtime',ROOT/'src/m68k']:
  for path in sorted(directory.glob('*')):
   if path.is_file():result[str(path)]=identity(path)
 for name in ['tests/model2a_sound_reference.h','tests/test_m68k_timing_reference.py','tests/test_segarally_sound_reset_reference.cpp','tests/test_segarally_sound_reset_reference.py','tests/test_segarally_sound_init_reference.cpp','tests/test_segarally_sound_init_reference.py']:
  path=ROOT/name;result[str(path)]=identity(path)
 for name in ['m68000.cpp','m68000.h','m68000.lst','m68000gen.py']:
  path=refs/name;result[str(path)]=identity(path) # The generator additionally checks fixed source pins.
 return result
def inventory(sound):
 program=(sound/'audiocpu.bin').read_bytes();require(len(program)==0x80000,'actual program size')
 text=(sound/'native-timed/snd_gen.cpp').read_text()
 blocks=re.findall(r'^L_([0-9a-f]+): // [^\n]+\n(.*?)(?=^L_|^}\n)',text,re.M|re.S)
 require(len(blocks)==2663,'actual generated address inventory')
 result=[]
 for address,body in blocks:
  pc=int(address,16)
  guards=re.findall(r'if \(c\.r16\((0x[0-9a-f]+)u\) != (0x[0-9a-f]+)u\) c\.fatal\("live sound code differs from recompiled image"\);',body)
  require(guards and [int(a,16) for a,w in guards]==list(range(pc,pc+len(guards)*2,2)),'contiguous live words')
  for a,w in guards:
   offset=int(a,16)-0x600000
   require(0<=offset<len(program)-1 and int.from_bytes(program[offset:offset+2],'big')==int(w,16),'live guard/actual ROM word')
  require(0x600100<=pc<=0x602f8a,'authenticated immutable-ROM PC range')
  result.append((pc,int(guards[0][1],16)))
 require(len(result)==2663 and len({op for pc,op in result})==660,'strict2663-PC/660-opcode inventory')
 return result
def prepare(args):
 out=args.output.resolve();sound=args.sound_dir.resolve();refs=args.reference_dir.resolve()
 require(out.is_relative_to(ROOT/'build'),'new ignored preparation directory required');out.mkdir(parents=True,exist_ok=False)
 before=source_closure(sound,refs);save(out/'inputs-before.json',before)
 profile=inventory(sound)
 (out/'opcodes.txt').write_text(''.join(f'{op:04x}\n' for op in sorted({op for pc,op in profile})))
 (out/'sound-reference-program.h').write_text('#pragma once\n#include <array>\n#include <utility>\n#include <cstdint>\nnamespace sound_reference {inline constexpr std::array<std::pair<uint32_t,uint16_t>,'+str(len(profile))+'> program_profile={{'+','.join('{0x%x,0x%x}'%(pc,op) for pc,op in profile)+'}};}\n')
 (out/'segarally-sound-reset-inputs.h').write_text('#pragma once\n#define SOUND_REFERENCE_NATIVE_SOURCE '+json.dumps(str(sound/'native-timed/snd_gen.cpp'))+'\n')
 compiler='/usr/bin/c++';commands=[]
 def run(command):
  commands.append(command)
  (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  subprocess.run(command,cwd=ROOT,check=True,timeout=180)
 run([sys.executable,'tests/test_m68k_timing_reference.py','--reference-dir',str(refs),'--output',str(out/'oracle'),'--board-opcodes',str(out/'opcodes.txt'),'--board-opcode-limit','660'])
 binary=out/'test-segarally-sound-init-reference'
 sources=['tests/test_segarally_sound_init_reference.cpp','src/runtime/model2a_sound.cpp','src/runtime/model2a_sound_bus.cpp','src/runtime/snd_cpu.cpp','src/runtime/model2_scsp.cpp','src/runtime/model2_scsp_dsp.cpp','src/runtime/model2a_clock.cpp']
 run([compiler,'-std=c++20','-O1','-g','-fno-fast-math','-ffp-contract=off','-Isrc','-I'+str(out),'-I'+str(out/'oracle'),*sources,'-o',str(binary)])
 require(before==source_closure(sound,refs),'preparation input mutation')
 outputs={str(path):identity(path) for path in out.rglob('*') if path.is_file()}
 prepared={'status':'PREPARED_NOT_EXECUTED','sound_dir':str(sound),'reference_dir':str(refs),'source_inputs':before,'prepared_artifacts':outputs,'binary':str(binary),'authenticated_inventory':[[hex(pc),hex(op)] for pc,op in profile],
 'bounds':{'instructions':1000000,'queue_boundaries':2000000,'virtual_ns':2000000000,'wall_seconds':180,'combined_log_bytes':16777216,'artifact_bytes':536870912},
 'stop':'Exactly1M completed instructions after actual startup enablesIRQs atPC6001f2/SR2000; require real SCSP reads/writes,IRQentry,RTE and PCMdrains. FirstSCSP write remains278301before/write100400/0300/ff00. No external UART input. Totals observed from current board, never old on_cycles probe.',
 'scope':'Independent persistent MAME CPU; separate instances of shared board/chip/queue/RNG implementations; no physical bus-phase/whole-recording/sound-on replay capability.'}
 save(out/'prepared.json',prepared);print('PREPARED ONLY; real program not executed:',out/'prepared.json')
def execute(args):
 prepared_dir=args.output.resolve();prepared=json.loads((prepared_dir/'prepared.json').read_text());out=args.run_dir.resolve()
 require(out.is_relative_to(ROOT/'build'),'new ignored run directory required');out.mkdir(parents=True,exist_ok=False)
 process=None;start=time.monotonic()
 try:
  def pins():
   require(prepared['source_inputs']==source_closure(Path(prepared['sound_dir']),Path(prepared['reference_dir'])),'frozen source/images changed')
   for path,item in prepared['prepared_artifacts'].items():require(identity(Path(path))==item,'prepared artifact changed: '+path)
  pins();command=[prepared['binary'],str(Path(prepared['sound_dir'])/'audiocpu.bin'),str(Path(prepared['sound_dir'])/'samples.bin'),str(out/'state.json')]
  save(out/'command.json',{'argv':command,'cwd':str(ROOT),'prepared_manifest':identity(prepared_dir/'prepared.json'),'bounds':prepared['bounds']})
  with (out/'stdout.txt').open('xb') as so,(out/'stderr.txt').open('xb') as se:
   process=subprocess.Popen(command,cwd=ROOT,stdout=so,stderr=se,start_new_session=True)
   while process.poll() is None:
    require(time.monotonic()-start<=180,'180s process-group watchdog')
    require(sum((out/name).stat().st_size for name in ['stdout.txt','stderr.txt'])<=16777216,'16MiB combined logs')
    require(sum(path.stat().st_size for path in out.rglob('*') if path.is_file())<=536870912,'512MiB outputs')
    time.sleep(.2)
  require(process.returncode==0,'first actual gate failed: exit '+str(process.returncode));pins()
  state=json.loads((out/'state.json').read_text());require(state['status']=='PASS' and state['instructions']==1000000 and state['initialization']['pc']=='6001f2' and state['IRQ_entries']>0 and state['RTE']>0,'exact checkpoint state')
  save(out/'result.json',{'status':'PASS','seconds':time.monotonic()-start,'inputs_unchanged':True,'state':state,'artifacts':{path.name:identity(path) for path in out.iterdir() if path.is_file()}});print('PASS:',out/'result.json')
 except BaseException as error:
  if process is not None and process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=3)
  save(out/'result.json',{'status':'FAIL','seconds':time.monotonic()-start,'exit_code':None if process is None else process.returncode,'error':str(error),'retry':'none'});raise
def main():
 parser=argparse.ArgumentParser(description=__doc__);mode=parser.add_mutually_exclusive_group(required=True);mode.add_argument('--prepare',action='store_true');mode.add_argument('--run',action='store_true')
 parser.add_argument('--output',type=Path,required=True);parser.add_argument('--sound-dir',type=Path);parser.add_argument('--reference-dir',type=Path);parser.add_argument('--run-dir',type=Path)
 args=parser.parse_args()
 if args.prepare:require(args.sound_dir and args.reference_dir and not args.run_dir,'prepare requires sound/ref directories only');prepare(args)
 else:require(args.run_dir and not args.sound_dir and not args.reference_dir,'run uses frozen preparation and new run directory only');execute(args)
if __name__=='__main__':main()
