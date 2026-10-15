#!/usr/bin/env python3
"""ROM-free coupled-board differential; separate MAME CPU, shared board models.

No game assets, SDL/device output, recording capability, or hardware claim.
Uses the current production sound emitter; never consumes an old native fixture.
"""
from pathlib import Path
import argparse, hashlib, json, os, shlex, struct, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference-dir',type=Path,default=ROOT/'build/validation/mame-reference')
p.add_argument('--output',type=Path,default=ROOT/'build/validation/model2a-sound-reference')
args=p.parse_args();out=args.output.resolve()
if not out.is_relative_to(ROOT/'build'): raise ValueError('ignored build output required')
out.mkdir(parents=True,exist_ok=False)
program=bytearray(0x80000);profile=[];cursor=0x100
def word(offset,value):struct.pack_into('>H',program,offset,value)
def longword(offset,value):struct.pack_into('>I',program,offset,value)
def emit(*words):
 global cursor
 profile.append((0x600000+cursor,words[0]))
 for value in words:word(cursor,value);cursor+=2
def move_word(value,address):emit(0x33fc,value,address>>16,address&65535)
longword(0,0x5000);longword(4,0x600100)
longword(8,0xffffffff);longword(12,0xffffffff) # Unsupported exception vectors: no accidental RAM-code seed.
emit(0x7200) # MOVEQ #0,D1: handler counter must not depend on reset-register values.
emit(0x23fc,0x60,0x300,0,25*4) # MOVE.L #handler, autovector25
for offset in range(0x100,0x110,2):move_word(0x1000,offset)
for offset,value in [(0x400,15),(2,0x100),(4,0),(6,4),(0xc,0x100),(0x16,0xe000),(0,0x1820),(0x424,8),(0x41e,8)]:
 move_word(value,0x100000+offset)
emit(0x46fc,0x2000) # MOVE #$2000,SR: unmask, retaining supervisor stack.
emit(0x4e71);emit(0x60fc) # NOP; BRA back to NOP
cursor=0x300
emit(0x3039,0x10,0x0404) # MOVE.W SCSP RX,D0; acknowledge ASSERT by consuming RX.
emit(0x33c0,0x10,0x0406) # MOVE.W D0,SCSP TX: real physical echo.
emit(0x5241)             # ADDQ.W #1,D1: retain interrupt count across RTE.
emit(0x4e73)
(out/'program.bin').write_bytes(program);(out/'seeds.txt').write_text('600300\n')
(out/'opcodes.txt').write_text(''.join(f'{op:04x}\n' for op in sorted({op for _,op in profile})))
(out/'sound-reference-program.h').write_text('#pragma once\n#include <array>\n#include <utility>\n#include <cstdint>\nnamespace sound_reference {\ninline constexpr std::array<std::pair<uint32_t,uint16_t>,'+str(len(profile))+'> program_profile={{'+','.join('{0x%x,0x%x}'%(pc,op) for pc,op in profile)+'}};\n}\n')
compiler=shlex.split(os.environ.get('CXX','c++'));flags=['-std=c++20','-O1','-g','-fno-fast-math','-ffp-contract=off','-Isrc']
commands=[]
def run(command):
 commands.append(command)
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 subprocess.run(command,cwd=ROOT,check=True)
run([sys.executable,'tests/test_m68k_timing_reference.py','--reference-dir',str(args.reference_dir.resolve()),'--output',str(out/'oracle'),'--board-opcodes',str(out/'opcodes.txt')])
run(compiler+flags+['tools/m2sndrecomp/main.cpp','src/m68k/decode.cpp','src/m68k/reach.cpp','src/m68k/timing.cpp','-o',str(out/'m2sndrecomp')])
run([str(out/'m2sndrecomp'),str(out/'program.bin'),str(out/'native.cpp'),'--board','model2a','--timing','model2a-boundary','--seeds',str(out/'seeds.txt'),'--audit',str(out/'native-audit.json')])
audit=json.loads((out/'native-audit.json').read_text())
assert audit['instructions']==len(profile) and audit['missing_templates']==audit['missing_timing']==0
assert not audit['undefined_sites'] and not audit['unmapped_sites'] and audit['live_code_guard']
sources=['tests/test_model2a_sound_reference.cpp',str(out/'native.cpp'),'src/runtime/model2a_sound.cpp','src/runtime/model2a_sound_bus.cpp','src/runtime/snd_cpu.cpp','src/runtime/model2_scsp.cpp','src/runtime/model2_scsp_dsp.cpp','src/runtime/model2a_clock.cpp','src/runtime/model2_uart.cpp','src/runtime/model2a_uart.cpp']
run(compiler+flags+['-I'+str(out),'-I'+str(out/'oracle')]+sources+['-o',str(out/'test-model2a-sound-reference')])
run([str(out/'test-model2a-sound-reference'),str(out/'program.bin')])
(out/'audit.json').write_text(json.dumps({'status':'PASS','scope':'Independent MAME M68000 persistent instruction/reset/IRQ execution, separate queues/bus RAM/SCSP/RNG and physical UART link, under shared instruction-start MMIO board contract. No independent SCSP/per-bus-phase/whole-game/hardware claim. Sound-on recording validation remains unsupported.','synthetic_instruction_addresses':len(profile),'public_opcodes':len({op for _,op in profile}),'generated_files':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in ['program.bin','native.cpp','native-audit.json','oracle/m68000-board-reference.h','test-model2a-sound-reference']}},indent=2)+'\n')
