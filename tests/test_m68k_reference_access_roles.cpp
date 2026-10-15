// ROM-free observer-purpose fixture. Public opcode bytes only. Compare an
// unchanged provider with the opt-in purpose observer, including CPU state,
// RAM, cycle deductions and every physical access except the evidence label.
#include <cstdint>
#include <vector>
#include <array>
#include <bit>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <tuple>
namespace plain {
#include "plain/m68000-board-reference.h"
}
namespace tagged {
#include "tagged/m68000-board-reference.h"
}
namespace {
unsigned checks=0;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
template<class C> auto state(const C &c){return std::tuple{c.m_ipc,c.m_pc,c.m_au,c.m_at,c.m_aob,c.m_dt,c.m_int_vector,c.m_sp,c.m_icount,c.m_bcount,c.m_count_before_instruction_step,c.m_t,c.m_movems,c.m_isr,c.m_sr,c.m_new_sr,c.m_dbin,c.m_dbout,c.m_edb,c.m_irc,c.m_ir,c.m_ird,c.m_ftu,c.m_aluo,c.m_alue,c.m_alub,c.m_movemr,c.m_irdi,c.m_base_ssw,c.m_ssw,c.m_dcr,c.m_int_next_state,c.m_next_state,c.m_inst_state,c.m_inst_substate,c.cycle_origin,c.last_vpa_time};}
// Compare the register array separately; the tuple contains scalar microstate.
template<class A,class B>void compare(const A &a,const B &b){
 require(std::equal(std::begin(a.m_da),std::end(a.m_da),std::begin(b.m_da)),"all D/A/USP/SSP registers unchanged");
 require(state(a)==state(b),"CPU microstate/prefetch/SR/cycles unchanged");
 require(a.memory==b.memory,"all sixteen MiB fixture memory unchanged");require(a.accesses.size()==b.accesses.size(),"all bus operations retained");
 for(size_t i=0;i<a.accesses.size();++i){const auto &x=a.accesses[i];const auto &y=b.accesses[i];require(x.address==y.address&&x.data==y.data&&x.mask==y.mask&&x.cycle==y.cycle,"bus address/value/mask/phase unchanged");require(x.kind==y.kind||(x.kind=='p'&&y.kind=='o'),"only PROGRAM purpose label can differ");}
}
struct Observed {char kind;uint32_t address;uint16_t value,mask;bool write;uint64_t phase;};
template<class C>void observe(C &cpu,std::vector<Observed> &rows){
 cpu.mapped_access=[&cpu,&rows](char kind,uint32_t address,uint16_t data,uint16_t mask,bool write){
  require(rows.size()<128,"bounded mapped callback rows");const auto aligned=address&~1u;require(aligned+1<cpu.memory.size(),"public fixture callback range");
  const uint16_t old=uint16_t(cpu.memory[aligned]<<8|cpu.memory[aligned+1]);const uint16_t value=kind=='i'?uint16_t(24+(cpu.m_next_state>>24)):write?uint16_t(data&mask):uint16_t(old&mask);
  rows.push_back({kind,address,value,mask,write,cpu.cycle_origin+uint64_t(100000000-cpu.m_icount)});
  if(write){const uint16_t merged=uint16_t((old&~mask)|(data&mask));cpu.memory[aligned]=uint8_t(merged>>8);cpu.memory[aligned+1]=uint8_t(merged);}
  return value;
 };
}
template<class A,class B>void callbacks(const A &a,const B &b,const std::vector<Observed> &x,const std::vector<Observed> &y){
 require(x.size()==a.accesses.size()&&y.size()==b.accesses.size()&&x.size()==y.size(),"mapped callback retains every access");
 for(size_t i=0;i<x.size();++i){require((x[i].write?'w':x[i].kind)==a.accesses[i].kind&&(y[i].write?'w':y[i].kind)==b.accesses[i].kind,"callback purpose agrees with internal ledger");require(x[i].address==y[i].address&&x[i].value==y[i].value&&x[i].mask==y[i].mask&&x[i].write==y[i].write&&x[i].phase==y[i].phase,"mapped callback preserves actual values/address/lanes/phase");require(x[i].kind==y[i].kind||(x[i].kind=='p'&&y[i].kind=='o'),"callback changes only program operand purpose");}
}
struct Case {const char *name;uint16_t opcode,extension;unsigned operand_bytes;uint32_t operand;bool indexed=false,absolute=false,immediate=false;};
void exercise(const Case &test){
 plain::MicroOracle a;tagged::MicroOracle b;std::vector<Observed> aa,bb;observe(a,aa);observe(b,bb);
 const auto setup=[&](auto &cpu){
  cpu.waits=true;cpu.word(0x600100,test.opcode);cpu.word(0x600102,test.extension);cpu.word(0x600104,0x4e71);cpu.word(0x600106,0x4e71);cpu.word(0x600108,0x4e71);
  cpu.m_da[9]=0x2000;cpu.m_da[5]=0x30;cpu.m_da[0]=0x87654321;
  if(test.indexed)cpu.word(0x600104,0x000e);
  if(test.absolute)cpu.word(0x600104,0x2000);
  if(test.immediate)cpu.word(0x600104,0x5678);
  if(test.operand!=0x600104&&test.operand_bytes){cpu.word(test.operand&~1u,0x12a5);if(test.operand_bytes==4)cpu.word(test.operand+2,0x5678);}
  cpu.setup(test.opcode);cpu.run(test.opcode);
 };
 setup(a);setup(b);compare(a,b);callbacks(a,b,aa,bb);
 unsigned operands=0,stream=0;
 for(const auto &row:b.accesses){if(row.kind=='o'){
  ++operands;require(!test.absolute&&!test.immediate&&test.operand_bytes>0,"only PC-relative operand uses purpose o");
  require(row.address==(test.operand&~1u)||(test.operand_bytes==4&&row.address==test.operand+2),"all PC-relative operand words retained");
  if(test.operand_bytes==1)require(row.mask==(test.operand&1?0x00ff:0xff00),"PC-relative byte lane retained");
 }else if(row.kind=='p')++stream;}
 require(operands==(test.absolute?0u:(test.operand_bytes+1)/2),"exact independently specified operand read count");require(stream>0,"instruction stream reads remain separate");
 if(test.indexed)require(b.memory[0x200e]==0x12&&b.memory[0x200f]==0xa5,"actual337b source copied to RAM");
 else if(test.operand_bytes&&!test.absolute){const uint32_t expected=test.operand_bytes==1?0xa5:test.operand_bytes==4?0x12a55678:test.operand==0x600104?0x4e71:0x12a5;const auto mask=test.operand_bytes==1?0xffu:test.operand_bytes==2?0xffffu:0xffffffffu;require((b.m_da[0]&mask)==expected,"PC-relative source architectural result");}
 if(test.immediate){require(b.m_da[0]==0x12345678,"long immediate result");require(std::any_of(b.accesses.begin(),b.accesses.end(),[](const auto &row){return row.kind=='p'&&row.address==0x600104;}),"DBIN-only long immediate remains instruction stream");}
 if(test.absolute){require(b.m_da[0]==0x12a55678,"absolute source result");require(std::any_of(b.accesses.begin(),b.accesses.end(),[](const auto &row){return row.kind=='p'&&row.address==0x600104;}),"DBIN-only absolute extension remains instruction stream");}
 if(test.operand==0x600104)require(std::any_of(b.accesses.begin(),b.accesses.end(),[](const auto &row){return row.kind=='p'&&row.address==0x600104;}),"same address may be operand and instruction stream");
 std::printf("PASS %s cycles=%d operands=%u stream=%u\n",test.name,100000000-b.m_icount,operands,stream);
}
void states(){
 plain::MicroOracle a;tagged::MicroOracle b;std::vector<Observed> aa,bb;observe(a,aa);observe(b,bb);
 const auto reset=[](auto &cpu){cpu.waits=true;cpu.word(0,0);cpu.word(2,0x4000);cpu.word(4,0x60);cpu.word(6,0x0100);cpu.word(0x600100,0x4e71);cpu.word(0x600102,0x4e71);cpu.state_reset();};
 reset(a);reset(b);compare(a,b);callbacks(a,b,aa,bb);require(std::none_of(b.accesses.begin(),b.accesses.end(),[](const auto &r){return r.kind=='o';}),"reset program words never become PC-relative operands");
 const auto irq=[](auto &cpu){cpu.accesses.clear();cpu.m_icount=100000000;cpu.m_sr=0x2000;cpu.word(25*4,0x0060);cpu.word(25*4+2,0x0200);cpu.word(0x600200,0x4e71);cpu.word(0x600202,0x4e71);cpu.m_next_state=(1u<<24)|cpu.S_INTERRUPT;cpu.state_interrupt();};
 aa.clear();bb.clear();irq(a);irq(b);compare(a,b);callbacks(a,b,aa,bb);require(std::count_if(b.accesses.begin(),b.accesses.end(),[](const auto &r){return r.kind=='i';})==1,"CPU-space IACK remains separate");require(std::none_of(b.accesses.begin(),b.accesses.end(),[](const auto &r){return r.kind=='o';}),"IRQ vector/refill never becomes PC-relative operand");
 std::puts("PASS reset/IACK purpose and unchanged architectural state");
}
}
int main(){try{
 for(const auto &test:std::array{
  Case{"actual337b indexed word to RAM",0x337b,0x5008,2,0x60013a,true},
  Case{"displaced PC word",0x303a,0x003e,2,0x600140},
  Case{"displaced PC byte odd lane",0x103a,0x003f,1,0x600141},
  Case{"displaced PC long",0x203a,0x003e,4,0x600140},
  Case{"operand aliases stream word",0x303a,0x0002,2,0x600104},
  Case{"DBIN-only long immediate",0x203c,0x1234,0,0,false,false,true},
  Case{"DBIN-only absolute extension",0x2039,0x0000,4,0x2000,false,true},
  Case{"ordinary NOP refill",0x4e71,0x4e71,0,0},
  Case{"taken branch discarded refill",0x6000,0x0010,0,0}
 })exercise(test);
 states();
 std::printf("PASS reference access-purpose observer: %u checks\n",checks);return 0;
 }catch(const std::exception &error){std::fprintf(stderr,"FAIL reference access-purpose observer: %s\n",error.what());return 1;}}
