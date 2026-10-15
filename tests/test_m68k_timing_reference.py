#!/usr/bin/env python3
"""Differential native state/timing check against pinned public M68000 microcode.

Requires the four pinned MAME sources under ignored build/validation.
Generated reference helpers are BSD-3-Clause, copyright Olivier Galibert.
No game images are consulted and generated reference code stays under build/.
Always regenerate the native fixture with the current production emitter;
an older header can retain obsolete cycle and ordered-write behavior.
"""
from pathlib import Path
import runpy,sys,re
import argparse, hashlib, json, subprocess, os, shlex
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reference-dir',type=Path,default=ROOT/'build/validation/mame-reference')
parser.add_argument('--output',type=Path,default=ROOT/'build/validation/m68000-timing-reference')
parser.add_argument('--sanitize',action='store_true')
parser.add_argument('--division-only',action='store_true',help='exhaustive divisor/boundary matrix using independent microcode and emitted native instructions')
parser.add_argument('--board-opcodes',type=Path,help='test-only live-bus provider header for these public opcode words; generate only')
parser.add_argument('--board-opcode-limit',type=int,choices=(64,660),default=64,
 help='explicit test-only provider bound; 660 admits an externally authenticated real-program inventory, not whole-recording support')
args=parser.parse_args()
if args.board_opcode_limit!=64 and not args.board_opcodes: parser.error('--board-opcode-limit requires --board-opcodes')
refs=args.reference_dir.resolve()
out=args.output.resolve()
if not out.is_relative_to(ROOT/'build'): raise ValueError('reference output must be ignored build output')
out.mkdir(parents=True,exist_ok=True)
hashes={
'm68000.cpp':'a1d4daea1f61cdc4779822c7e59d561ff48c7337aaa7f920854fe2c624d70883',
'm68000.h':'cdfb07d21b62dbf042cdebccd0d57d1be61cf84b8be257b3173408ab307425fb',
'm68000.lst':'9b4605ef5e0bd2bb2e17a03146c37c6cf4757cf926ab1b06352d95ab09695631',
'm68000gen.py':'87bcba366263ae97daa88fce3a082e85563bc903df89f0692f589d668aced05b'}
for name,digest in hashes.items():
 if hashlib.sha256((refs/name).read_bytes()).hexdigest()!=digest:
  raise ValueError('pinned M68000 source mismatch: '+name)

sys.argv=['m68000gen.py','inspect',str(refs/'m68000.lst'),'unused']
g=runpy.run_path(str(refs/'m68000gen.py'))
mode=g['GEN'].direct|g['GEN'].full
# Public instruction patterns only: no game ROM or generated game source.
ops={op | (2 if mask==0xff00 else 0) for op,mask,_ in g['instructions']}
if args.division_only: ops={0x80c1,0x81c1}
if args.board_opcodes:
 if args.division_only: raise ValueError('board provider and division matrix are separate modes')
 ops={int(line,16) for line in args.board_opcodes.read_text().splitlines()}
 if not ops or len(ops)>args.board_opcode_limit or any(op<0 or op>65535 for op in ops): raise ValueError('bounded public opcode list required')
head=(refs/'m68000.h').read_text()
helpers=head[head.index('\t// movem step'):head.rindex('\n};')].replace('\tvoid set_ftu_const();','')
source=(refs/'m68000.cpp').read_text()
ftu=source[source.index('void m68000_device::set_ftu_const()'):source.index('void m68000_device::start_interrupt_vector_lookup()')].replace('m68000_device::','')
pre=r'''
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <bit>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;
using s8=int8_t;using s16=int16_t;using s32=int32_t;
struct MicroOracle {
 enum {S_RESET,S_BUS_ERROR,S_ADDRESS_ERROR,S_DOUBLE_FAULT,S_INTERRUPT,S_TRACE,S_ILLEGAL,S_PRIVILEDGE,S_LINEA,S_LINEF};
 enum {SR_C=1,SR_V=2,SR_Z=4,SR_N=8,SR_X=16,SR_I=0x700,SR_S=0x2000,SR_T=0x8000,SR_CCR=31,SR_SR=0xa700};
 enum {SSW_DATA=1,SSW_PROGRAM=2,SSW_CPU=3,SSW_S=4,SSW_N=8,SSW_R=16,SSW_CRITICAL=32};
 u32 m_da[17]{};
 u32 m_ipc=0,m_pc=0,m_au=0,m_at=0,m_aob=0,m_dt=0,m_int_vector=0,m_sp=16;
 int m_icount=100000000,m_bcount=0,m_count_before_instruction_step=0,m_t=0;
 u32 m_movems=0;
 u16 m_isr=0,m_sr=0x2700,m_new_sr=0,m_dbin=0,m_dbout=0,m_edb=0;
 u16 m_irc=0,m_ir=0,m_ird=0,m_ftu=0,m_aluo=0,m_alue=0,m_alub=0,m_movemr=0,m_irdi=0;
 u16 m_base_ssw=0,m_ssw=0;u8 m_dcr=0;
 u32 m_int_next_state=0,m_next_state=0,m_inst_state=0,m_inst_substate=0;
 std::array<u16,65536> m_decode_table{};
 std::vector<u8> memory=std::vector<u8>(0x1000000);
 bool waits=false;u64 cycle_origin=0,last_vpa_time=0;
 struct Access {char kind;u32 address;u16 data,mask;u64 cycle;};
 std::vector<Access> accesses;
 struct Port {MicroOracle&cpu;char kind;
  u16 read_interruptible(u32 a,u16 mask=0xffff){return cpu.access(kind,a,0,mask,false);}
  void write_interruptible(u32 a,u16 value,u16 mask=0xffff){cpu.access(kind,a,value,mask,true);}
 } m_program{*this,'d'},m_opcodes{*this,'p'},m_cpu_space{*this,'i'};
 u16 access(char kind,u32 address,u16 data,u16 mask,bool write){
  address&=0xffffff;
  if(kind=='i') {
   const u64 current=cycle_origin+100000000-m_icount;
   if(last_vpa_time<current)last_vpa_time=current-current%10+(current%10<7?10:20);
   m_icount-=int(last_vpa_time-current)+1;
  } else if(waits && (address<0x80000 || (address>=0x100000 && address<0x101000)))--m_icount;
  accesses.push_back({write?'w':kind,address,data,mask,cycle_origin+100000000-m_icount});
  if(kind=='i')return u16(24+(m_next_state>>24));
  address&=~1u;
  const u16 old=u16(memory[address]<<8|memory[address+1]);
  if(write){data=u16((old&~mask)|(data&mask));memory[address]=u8(data>>8);memory[address+1]=u8(data);}
  return old&mask;
 }
 void word(u32 address,u16 value){address&=0xffffff;memory[address]=u8(value>>8);memory[address+1]=u8(value);}
 void setup(u16 opcode,u32 pc=0x600100){m_ipc=pc;m_pc=pc+2;m_au=pc+4;m_ir=m_ird=m_irdi=opcode;
  word(pc,opcode);m_irc=m_dbin=u16(memory[pc+2]<<8|memory[pc+3]);set_ftu_const();m_icount=100000000;accesses.clear();}
 bool access_to_be_redone(){return false;}
 void update_user_super(){m_sp=(m_sr&SR_S)?16:15;}
 void update_interrupt(){}
 void start_interrupt_vector_lookup(){}
 void end_interrupt_vector_lookup(){m_int_vector=(m_edb&255)<<2;m_int_next_state=0;}
 struct Callback {bool isnull(){return true;}template<class...T>int operator()(T...){return 0;}} m_rte_instr_callback,m_cmpild_instr_callback,m_tas_write_callback;
 template<class...T>void m_reset_cb(T...){ }
 template<class...T>void debugger_exception_hook(T...){ }
 void debugger_wait_hook(){ }
'''
if args.board_opcodes:
 # Only this harness-owned scaffold changes. Pinned generated microcode,
 # helper arithmetic and all its cycle deductions remain untouched. Default
 # generation below is byte-identical to the ordinary state/timing fixture.
 pre=pre.replace('#include <algorithm>','#include <algorithm>\n#include <functional>')
 pre=pre.replace('std::vector<Access> accesses;', 'std::vector<Access> accesses;\n std::function<u16(char,u32,u16,u16,bool)> mapped_access;')
 pre=pre.replace("if(kind=='i')return u16(24+(m_next_state>>24));", "if(kind=='i'){if(mapped_access)mapped_access(kind,address,data,mask,write);return u16(24+(m_next_state>>24));}\n  if(mapped_access)return mapped_access(kind,address,data,mask,write);")
parts=[pre,helpers,ftu]
for opcode in sorted(ops):
 code=g['generate_intermediate_code_from_instruction'](opcode,0xffff,'test',mode)
 parts.append('void op_%04x(){\n'%opcode+'\n'.join(g['generate_source_from_code'](code,mode))+'\n}\n')
for state in (g['states'][0],g['states'][4]):
 code=g['generate_intermediate_code_from_state'](state,mode)
 parts.append('void state_%s(){\n'%state[1]+'\n'.join(g['generate_source_from_code'](code,mode))+'\n}\n')
parts.append('void run(u16 opcode){switch(opcode){\n'+''.join('case 0x%04x:op_%04x();break;\n'%(op,op)for op in sorted(ops))+'default:throw std::runtime_error("oracle opcode missing");}}\n};\n')
parts.append('inline constexpr uint16_t reference_opcodes[]={'+','.join(hex(op)for op in sorted(ops))+'};')
header=out/('m68000-board-reference.h' if args.board_opcodes else 'm68000-timing-reference.h')
header.write_text('// Generated from pinned public BSD-3-Clause M68000 microcode; Olivier Galibert.\n'+'\n'.join(parts))
if args.board_opcodes:
 print(len(ops),'live-bus microcode handlers generated; no compilation/execution')
 raise SystemExit(0)
compiler=shlex.split(os.environ.get('CXX','c++'))
flags=['-std=c++20','-O1','-g','-Isrc','-I'+str(out)]
if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
def run(command): subprocess.run(command,cwd=ROOT,check=True)
fixture=out/'generate-native-fixture'
run(compiler+flags+['tests/m68k_timing_fixture.cpp','src/m68k/decode.cpp','src/m68k/reach.cpp','src/m68k/timing.cpp','-o',str(fixture)])
run([str(fixture),str(out/'m68000-native-fixture.h')])
exe=out/'test-m68000-reference'
test_source='tests/test_m68k_division_reference.cpp' if args.division_only else 'tests/test_m68k_timing_reference.cpp'
run(compiler+flags+[test_source,'src/m68k/decode.cpp','src/m68k/timing.cpp','src/runtime/snd_cpu.cpp','-o',str(exe)])
run([str(exe)])
(out/'audit.json').write_text(json.dumps({'reference_commit':'dddd73680656e355bb2b5beecab1167c9f07bf81','reference_sha256':hashes,'public_opcode_patterns':len(ops),'sanitized':args.sanitize,'division_only':args.division_only,'scope':('Production-emitted DIVU/DIVS total cycles, results and defined flags against independent microcode for all nonzero divisors and boundary/random dividends. Zero-divide exception delivery remains unsupported.' if args.division_only else 'Instruction total cycles, mapped waits, total bus transfer counts, ordered write address/data/masks, completed architectural registers/stack banks/PC/defined SR and complete synthetic memory; reset and phase-dependent autovector entry. DIV overflow N/Z and reset undefined state are excluded. Existing instruction cases start supervisor with X=0; IRQ cases start supervisor. Bus-stage timing, prefetch values, other exceptions, SCSP and coupled-board fidelity are not claimed.')},indent=2)+'\n')
print(len(ops),'microcode oracle handlers generated')
