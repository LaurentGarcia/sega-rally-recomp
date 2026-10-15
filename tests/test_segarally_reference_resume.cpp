// ROM-free actual Machine versus independent opcode interpreter phase tests.
#include "tools/srally_boot/reference_resume.h"
#include "runtime/model2a_clock.h"
#include "runtime/model2a_control.h"
#include "runtime/gen_support.h"
#include "runtime/tgp.h"
#include "runtime/m2_replay_bus.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>

namespace {
namespace rr = srally::reference_resume;
namespace hi = srally::host_input;
using M = rt::Model2AMachine;
constexpr uint32_t PRCB=0x200000,TABLE=0x200200,STACK=0x201800;
constexpr uint64_t FRAME_END=(rt::Model2AScreen::FRAME_NS+rt::Model2AScreen::VBLANK_NS)/40;
unsigned checks=0;
void check(bool condition,const char *message) {++checks;if(!condition)throw rt::Fatal(message);}
template<class F> void rejects(F function) {bool failed=false;try{function();}catch(const std::exception&){failed=true;}check(failed,"unsupported operation accepted");}
struct Code {
 std::map<uint32_t,rr::GuardedInstruction> instructions;
 void lda(uint32_t pc,uint32_t value,bool extended=false){instructions[pc]={{extended?0x8c803000u:0x8c800000u|value,value},extended?2u:1u};}
 void ld(uint32_t pc,uint32_t address){instructions[pc]={{0x90803000,address},2};}
 M::Images images()const {
  M::Images i;i.program.resize(0x2000);auto put=[&](uint32_t p,uint32_t w){for(unsigned n=0;n<4;++n)i.program[p+n]=uint8_t(w>>(8*n));};
  put(4,PRCB);put(12,instructions.begin()->first);for(auto [pc,insn]:instructions)for(unsigned n=0;n<insn.count;++n)put((pc>=0x5a0000?pc-0x5a0000+0x1000:pc)+4*n,insn.words[n]);
  i.tgp_tables.resize(0x40000);i.copro_data.resize(0x800000);i.polygons.resize(4);i.textures.resize(2);return i;
 }
};
const Code *active=nullptr;
M::Options options(bool hook){M::Options o;o.comm=o.uart=o.sound=o.external_video=false;o.instruction_boundary_hook=hook;return o;}
struct Ref {
 rt::Model2AClock clock;rt::Model2ABus bus;rt::I960Ref cpu;rt::Model2AControl control;rt::Model2AScreen screen;
 rt::Model2Eeprom eeprom;rt::Model2AIO io;std::deque<std::pair<unsigned,bool>> edges;uint64_t frames=0;bool syncing=false;
 Ref(const Code&code):bus(code.images().program,{}),cpu(&bus),control(clock),screen(clock),io(eeprom,[&]{return clock.now_ns();}) {
  eeprom.reset();bus.set_before_access([&]{sync();});bus.add_model(&control);bus.add_model(&io);
  control.on_irq_line=[&](unsigned line,bool level){edges.emplace_back(line,level);};
  screen.on_begin=[&](uint64_t,uint64_t){control.vblank();};screen.on_end=[&](uint64_t,uint64_t){++frames;};cpu.reset();
 }
 void sync(){if(syncing)return;syncing=true;try{clock.advance(cpu.reference_cycles*40);syncing=false;}catch(...){syncing=false;throw;}}
 void irq(){while(!edges.empty()){auto [line,level]=edges.front();edges.pop_front();cpu.execute_set_input(int(line),level);}cpu.check_immediate_irqs();}
 rr::Hooks hooks(const Code &code){return {[&]{sync();},[&]{irq();},[&]{return clock.now_ns();},[&]{return frames;},
  [&code](uint32_t pc)->std::optional<rr::GuardedInstruction>{auto i=code.instructions.find(pc);return i==code.instructions.end()?std::nullopt:std::optional(i->second);},&io.inputs};}
};
hi::Context context(bool hook){return {std::string(64,'e'),false,hook};}
constexpr hi::IndependentCpuCapabilities proven{true,true,true};
void initialize(rt::Cpu&c,bool irq=false){
 c.m_PRCB=PRCB;c.m_PC=0;c.m_ICR=0xf8f8f8f8;c.m_r[rt::I960_FP]=0x200800;c.m_r[rt::I960_SP]=0x200840;
 c.bus->write_dword(PRCB+20,TABLE);c.bus->write_dword(PRCB+24,STACK);c.bus->write_dword(TABLE+36+(0xf8-8)*4,0x500);
 if(irq){c.bus->write_dword(0xe80004,4);c.reference_cycles=2;(void)c.bus->read_dword(0xe80004);c.bus->write_dword(0xf00000,uint32_t(FRAME_END-7));c.reference_cycles=FRAME_END-6;}
}
bool same_cpu(const rt::Cpu&a,const rt::Cpu&b){
 // PIP is an interpreter decode diagnostic, not architectural state.
 return !std::memcmp(a.m_r,b.m_r,sizeof(a.m_r))&&!std::memcmp(a.m_fp,b.m_fp,sizeof(a.m_fp))&&
 !std::memcmp(a.m_rcache,b.m_rcache,sizeof(a.m_rcache))&&!std::memcmp(a.m_rcache_frame_addr,b.m_rcache_frame_addr,sizeof(a.m_rcache_frame_addr))&&
 !std::memcmp(a.m_irq_line_state,b.m_irq_line_state,sizeof(a.m_irq_line_state))&&
 std::tie(a.m_IP,a.m_PC,a.m_AC,a.m_SAT,a.m_PRCB,a.m_ICR,a.m_rcache_pos,a.m_immediate_irq,a.m_immediate_vector,a.m_immediate_pri,a.reference_cycles)==
 std::tie(b.m_IP,b.m_PC,b.m_AC,b.m_SAT,b.m_PRCB,b.m_ICR,b.m_rcache_pos,b.m_immediate_irq,b.m_immediate_vector,b.m_immediate_pri,b.reference_cycles);
}
// Shadows only fixture RAM, retaining authentic bus clocks/control devices.
// IRQ entry writes provide an ordered, cycle-stamped behavioral comparison.
struct RamTrace final : rt::DeviceModel {
 struct Write {uint32_t address,data,mask;uint64_t cycles;bool operator==(const Write&)const=default;};
 rt::Cpu &cpu;std::map<uint32_t,uint32_t> words;std::vector<Write> writes;bool injected=false;
 explicit RamTrace(rt::Cpu &c):cpu(c){}
 bool claims(uint32_t a)const override{return a>=0x200000&&a<0x220000;}
 Check read(uint32_t a,uint32_t,uint32_t &v)override{v=words[a];return Strict;}
 void write(uint32_t a,uint32_t d,uint32_t mask)override{
  words[a]=(words[a]&~mask)|(d&mask);writes.push_back({a,d,mask,cpu.reference_cycles});
  if(!injected&&a==STACK+64-16){injected=true;
   // Authentic queued clear/reassert edges produced DURING IRQ entry.
   cpu.bus->write_dword(0xe80000,~4u);cpu.bus->write_dword(0xf00000,0);
  }
 }
 uint16_t flags(uint32_t)const override{return rt::Cpu::BURST;}
 void measured_mismatch(uint32_t,uint32_t,uint32_t)override{throw rt::Fatal("RAM trace mismatch");}
};
hi::Resume resume(M&m,rr::Stepper&r,uint64_t index,uint64_t budget,rt::Model2AIO::Inputs pins={}){
 hi::Resume row;row.index=index;row.budget=budget;row.inputs=pins;row.before=hi::Witness::of(m.stats());m.inputs()=pins;
 const auto a=m.run_until_frame(budget);row.after=hi::Witness::of(m.stats());row.reason=a.reason;const auto b=r.consume(row);
 check(a.reason==b.reason&&a.pc==b.pc&&a.instructions==b.instructions,"resume return mismatch");
 check(row.after==r.witness(),"resume phase mismatch");return row;
}
void budget_and_fetch(bool hook){
 for(bool relocated:{false,true})for(bool extended:{false,true}){Code code;const uint32_t pc=relocated?0x5a0420:0x420;code.lda(pc,extended?0x12345678:1,extended);active=&code;M m(code.images(),options(hook));Ref ref(code);
 if(relocated)for(unsigned n=0;n<code.instructions.at(pc).count;++n){const auto w=code.instructions.at(pc).words[n];const_cast<rt::Cpu&>(m.cpu()).bus->write_dword(pc+n*4,w);ref.bus.write_dword(pc+n*4,w);}
 rr::Stepper r(ref.cpu,ref.hooks(code),context(hook),proven);
 auto row=resume(m,r,0,1);check(row.reason==M::Reason::Budget&&row.after.cycles==1&&row.after.board_time_ns==0,"Budget added final clock synchronization");check(same_cpu(m.cpu(),ref.cpu),"LDA CPU mismatch");check(ref.cpu.m_r[16]==(extended?0x12345678u:1u),"LDA value mismatch");
 auto frontier=resume(m,r,1,1);check(frontier.reason==M::Reason::MissingCode&&frontier.after.instructions==1&&frontier.after.board_time_ns==40,"next public resume failed to synchronize");}
 Code code;code.ld(0x420,0x200100);active=&code;M m(code.images(),options(hook));Ref ref(code);const_cast<rt::Cpu&>(m.cpu()).bus->write_dword(0x200100,0x87654321);ref.bus.write_dword(0x200100,0x87654321);rr::Stepper r(ref.cpu,ref.hooks(code),context(hook),proven);
 auto row=resume(m,r,0,1);check(row.after.cycles==4&&row.after.board_time_ns==160&&ref.cpu.m_r[16]==0x87654321,"genuine data read lost its charged clock");check(same_cpu(m.cpu(),ref.cpu),"data LD CPU mismatch");
}
void raw_fetch_difference(bool hook){
 Code code;code.lda(0x420,0x12345678,true);active=&code;M m(code.images(),options(hook));Ref ref(code);
 const auto result=m.run_until_frame(1);ref.sync();ref.irq();ref.cpu.execute_one();
 check(result.reason==M::Reason::Budget&&m.stats().board_time_ns==0&&ref.clock.now_ns()==40,
     "raw oracle long-displacement clock discrepancy disappeared");
 check(same_cpu(m.cpu(),ref.cpu),"raw long LDA architectural state should match despite clock lag");
}
void frame_budget_tie(bool hook){
 Code code;code.ld(0x420,0x200100);active=&code;M m(code.images(),options(hook));Ref ref(code);
 auto &cpu=const_cast<rt::Cpu&>(m.cpu());cpu.reference_cycles=ref.cpu.reference_cycles=FRAME_END-4;
 rr::Stepper stepper(ref.cpu,ref.hooks(code),context(hook),proven);auto row=resume(m,stepper,0,1);
 check(row.reason==M::Reason::Frame&&row.after.instructions==1&&row.after.frames==1,
     "Frame did not precede exact Budget tie during genuine data access");
 check(row.after.cycles==FRAME_END&&same_cpu(cpu,ref.cpu),"Frame/Budget tie state differs");
}
void irq_frame(bool hook){
 Code code;code.lda(0x420,1);code.ld(0x500,0x01c0001c);active=&code;M m(code.images(),options(hook));Ref ref(code);auto &cpu=const_cast<rt::Cpu&>(m.cpu());RamTrace native_trace(cpu),reference_trace(ref.cpu);
 static_cast<rt::Model2ABus*>(cpu.bus)->add_model(&native_trace);ref.bus.add_model(&reference_trace);
 initialize(cpu,true);initialize(ref.cpu,true);native_trace.writes.clear();reference_trace.writes.clear();rr::Stepper r(ref.cpu,ref.hooks(code),context(hook),proven);
 rt::Model2AIO::Inputs p;p.analog[0]=0x61;auto first=resume(m,r,0,1,p);p.analog[0]=0x72;auto frame=resume(m,r,1,2,p);
 check(first.reason==M::Reason::Budget&&frame.reason==M::Reason::Frame,"IRQ fixture did not yield Frame");check(frame.before.instructions==frame.after.instructions&&frame.after.cycles==frame.before.cycles+9&&frame.after.frames==1,"IRQ did not establish same-count frame");check(same_cpu(m.cpu(),ref.cpu),"IRQ/cache/CPU mismatch");
 check(native_trace.injected&&reference_trace.injected&&native_trace.writes==reference_trace.writes&&!native_trace.writes.empty(),"IRQ entry writes/cycle stamps differ");
 check(ref.edges.size()==2&&ref.cpu.m_irq_line_state[2]==1,"entry-generated IRQ edges were drained before next preparation");
 p.analog[0]=0x93;auto next=resume(m,r,2,1,p);check((ref.cpu.m_r[16]>>16&255)==0x93,"next public resume did not apply changed pins before opcode");check(next.after.instructions==2&&same_cpu(m.cpu(),ref.cpu),"post-IRQ opcode mismatch");
 check(ref.edges.empty()&&native_trace.writes==reference_trace.writes,"next preparation did not consume queued edges faithfully");
}
void seeds_and_pins(bool hook){
 Code code;std::vector<uint32_t> addresses={0x01c00000,0x01c00004,0x01c0000c};for(unsigned n=0;n<8;++n)addresses.push_back(0x01c0001c);addresses.push_back(0x01d00000);
 for(size_t n=0;n<addresses.size();++n)code.ld(0x420+uint32_t(n)*8,addresses[n]);active=&code;M m(code.images(),options(hook));Ref ref(code);
 std::vector<uint8_t> ee(128,0xff),backup(16384,0xff);ee[0]=0x34;ee[1]=0x12;backup[0]=0x21;backup[1]=0x43;backup[2]=0x65;backup[3]=0x87;m.load_eeprom(ee);m.load_backup(backup);rr::load_startup_seeds(ref.eeprom,ref.bus,ee,backup);
 const auto seeded=ref.eeprom.save_image();check(std::equal(ee.begin(),ee.end(),seeded.begin(),seeded.end())&&m.save_eeprom()==ee,"nondefault EEPROM seed lost");check(m.save_backup()==ref.bus.save_backup()&&m.save_backup()==backup,"nondefault backup seed lost");
 auto old_ee=ref.eeprom.save_image();auto old_backup=ref.bus.save_backup();rejects([&]{rr::load_startup_seeds(ref.eeprom,ref.bus,ee,std::span<const uint8_t>(backup.data(),2));});check(old_ee==ref.eeprom.save_image()&&old_backup==ref.bus.save_backup(),"bad seed sizes partially mutated startup state");
 rr::Stepper r(ref.cpu,ref.hooks(code),context(hook),proven);rt::Model2AIO::Inputs p;p.in0=0x96;p.in1=0x72;p.in2=0x39;p.switches=0x5a;p.analog={1,3,5,7,9,11,13,15};
 for(size_t n=0;n<addresses.size();++n){resume(m,r,n,1,p);uint32_t value=ref.cpu.m_r[16];
  if(n==0)check((value>>16&255)==p.in0,"in0 not observed");else if(n==1)check(value==(uint32_t(p.in2)<<16|p.in1),"in1/in2 not observed");else if(n==2)check((value&255)==p.switches,"switches not observed");else if(n<11)check((value>>16&255)==p.analog[n-3],"analog channel not observed");else check(value==0x87654321,"backup seed was not read by CPU");check(same_cpu(m.cpu(),ref.cpu),"all-pin CPU mismatch");}
}
void refusals(){
 Code code;code.lda(0x420,0x12345678,true);active=&code;Ref ref(code);auto hooks=ref.hooks(code);auto sound=context(true);sound.sound=true;rejects([&]{rr::Stepper r(ref.cpu,hooks,sound,proven);});check(ref.cpu.reference_cycles==0&&ref.cpu.m_IP==0x420,"sound rejection executed CPU");
 rejects([&]{rr::Stepper r(ref.cpu,hooks,context(true),{});});
 hooks.guarded_instruction=[](uint32_t){return std::optional(rr::GuardedInstruction{{0x8c803000,0x12345678},1});};
 {rr::Stepper r(ref.cpu,hooks,context(true),proven);rejects([&]{r.run_until_frame(1,{});});check(ref.cpu.reference_cycles==0,"bad length profile charged CPU");rejects([&]{r.run_until_frame(1,{});});}
 hooks=ref.hooks(code);hooks.guarded_instruction=[](uint32_t){return std::optional(rr::GuardedInstruction{{0x8c803000,0x12345679},2});};
 {rr::Stepper r(ref.cpu,hooks,context(true),proven);rejects([&]{r.run_until_frame(1,{});});check(ref.cpu.reference_cycles==0&&ref.cpu.m_IP==0x420,"bad live word executed CPU");}
 for(unsigned count:{0u,3u}){hooks=ref.hooks(code);
  hooks.guarded_instruction=[count](uint32_t){return std::optional(rr::GuardedInstruction{{0x8c803000,0x12345678},count});};
  rr::Stepper r(ref.cpu,hooks,context(true),proven);rejects([&]{r.run_until_frame(1,{});});
  check(ref.cpu.reference_cycles==0&&ref.cpu.m_IP==0x420,"invalid fetch count executed CPU");rejects([&]{r.run_until_frame(1,{});});}
 hooks=ref.hooks(code);
 {rr::Stepper r(ref.cpu,hooks,context(true),proven);hi::Resume fault;fault.reason=M::Reason::DeviceAccess;fault.before=r.witness();rejects([&]{r.consume(fault);});check(ref.cpu.reference_cycles==0,"unsupported captured fault executed CPU");}
}
void data_fault(bool hook){
 Code code;code.ld(0x420,0x20000000);active=&code;M m(code.images(),options(hook));Ref ref(code);rr::Stepper r(ref.cpu,ref.hooks(code),context(hook),proven);
 const auto actual=m.run_until_frame(1);bool caught=false;
 try{r.run_until_frame(1,{});}catch(const rt::Model2AAccess &fault){caught=true;
  check(fault.address==actual.address&&fault.width==actual.width&&fault.write==actual.write&&fault.value==actual.value,
      "genuine data fault metadata was changed by instruction caching");}
 check(caught&&actual.reason==M::Reason::DeviceAccess,"data fault did not propagate");
 check(m.stats().instructions==0&&r.witness().instructions==0&&m.stats().reference_cycles==4&&ref.cpu.reference_cycles==4&&
     m.stats().board_time_ns==160&&ref.clock.now_ns()==160,"data fault lost charged-cycle synchronization");
 // The independent interpreter has advanced IP to decode its operand. This
 // unproven fault-state difference is not normalized away or accepted as parity.
 check(m.cpu().m_IP!=ref.cpu.m_IP,"fault fixture no longer demonstrates excluded interpreter IP phase");
 rejects([&]{r.run_until_frame(1,{});});
}
struct FaultBus final : rt::Bus {
 rt::Cpu *cpu=nullptr;std::vector<std::pair<uint32_t,uint64_t>> fetches;
 uint32_t fetch(uint32_t a)override{fetches.emplace_back(a,cpu->reference_cycles);if(a==0x5a0424)throw rt::Model2AAccess(a,4,false,0);return 0x8c803000;}
 uint8_t read_byte(uint32_t)override{throw rt::Fatal("unexpected data access");}
 uint16_t read_word(uint32_t)override{throw rt::Fatal("unexpected data access");}
 uint32_t read_dword(uint32_t)override{throw rt::Fatal("unexpected data access");}
 void write_byte(uint32_t,uint8_t)override{throw rt::Fatal("unexpected data access");}
 void write_word(uint32_t,uint16_t)override{throw rt::Fatal("unexpected data access");}
 void write_dword(uint32_t,uint32_t)override{throw rt::Fatal("unexpected data access");}
};
void guarded_fetch_fault(){
 FaultBus bus;rt::I960Ref cpu(&bus);bus.cpu=&cpu;cpu.m_IP=0x5a0420;rt::Model2AIO::Inputs inputs;
 rr::Hooks hooks{[]{},[]{},[]{return uint64_t(0);},[]{return uint64_t(0);},
  [](uint32_t){return std::optional(rr::GuardedInstruction{{0x8c803000,0x12345678},2});},&inputs};
 rr::Stepper r(cpu,hooks,context(true),proven);bool caught=false;
 try{r.run_until_frame(1,{});}catch(const rt::Model2AAccess&e){caught=true;check(e.address==0x5a0424&&e.width==4&&!e.write,"second-word guard fault changed");}
 check(caught&&cpu.reference_cycles==0&&cpu.m_IP==0x5a0420&&r.witness().instructions==0,"guard fault executed an opcode");
 check(bus.fetches==std::vector<std::pair<uint32_t,uint64_t>>{{0x5a0420,0},{0x5a0424,0}},"guard accesses were moved after charging");
 rejects([&]{r.run_until_frame(1,{});});
}
void relocated_guard_mismatch(){
 Code code;code.lda(0x5a0420,0x12345678,true);active=&code;M m(code.images(),options(true));Ref ref(code);
 for(auto *cpu:{&const_cast<rt::Cpu&>(m.cpu()),static_cast<rt::Cpu*>(&ref.cpu)}){
  cpu->bus->write_dword(0x5a0420,0x8c803000);cpu->bus->write_dword(0x5a0424,0x12345679);}
 rr::Stepper r(ref.cpu,ref.hooks(code),context(true),proven);const auto result=m.run_until_frame(1);
 rejects([&]{r.run_until_frame(1,{});});
 check(result.reason==M::Reason::RuntimeError&&m.stats().instructions==0&&m.stats().reference_cycles==0&&
     r.witness().instructions==0&&ref.cpu.reference_cycles==0,"changed live second word executed an opcode");
 rejects([&]{r.run_until_frame(1,{});});
}
} // namespace
namespace gen {
bool instruction_boundary_hook_compatible(){return true;}bool has_code(uint32_t pc){return active&&active->instructions.count(pc);}uint64_t native_instructions(){return active->instructions.size();}
void run(Env&e){while(!e.ls.boundary_yielded()&&has_code(e.c.m_IP)&&!e.ls.boundary()){
 auto pc=e.c.m_IP;auto insn=active->instructions.at(pc);if(pc>=0x5a0000)for(unsigned n=0;n<insn.count;++n)if(e.c.bus->read_dword(pc+n*4)!=insn.words[n])throw rt::Fatal("synthetic live guard");
 if((insn.words[0]>>24)==0x90){e.c.consume_reference_cycles(4);e.c.m_r[16]=e.c.bus->read_dword(insn.words[1]);}
 else{e.c.consume_reference_cycles(1);e.c.m_r[16]=insn.count==2?insn.words[1]:insn.words[0]&0xfff;}
 e.c.m_IP+=insn.count*4;++e.ls.count;
}}
}
namespace rt::tgpgen {extern const uint32_t program_crc32=0,program_words=0;void run(Tgp&,uint64_t){throw TgpFatal("synthetic phase test must not run TGP");}}
int main(){try{for(bool hook:{false,true}){raw_fetch_difference(hook);budget_and_fetch(hook);frame_budget_tie(hook);irq_frame(hook);seeds_and_pins(hook);data_fault(hook);}refusals();guarded_fetch_fault();relocated_guard_mismatch();std::printf("Ordered reference resumes: %u checks passed\n",checks);return 0;}catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
