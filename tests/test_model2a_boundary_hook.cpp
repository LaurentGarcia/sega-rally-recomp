// Synthetic old/new Model2AMachine differential. No game ROM or decoder.
// Authoring checkpoint: do not execute during the live Mac playtest.
#include "runtime/model2a_machine.h"
#include "runtime/model2a_clock.h"
#include "runtime/gen_support.h"
#include "runtime/tgp.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <map>
#include <tuple>
#include <vector>
#include <cstdio>

#ifndef M2_INSTRUCTION_BOUNDARY_HOOK
#error This differential must compile both old/new paths into the same exact binary.
#endif
namespace {
using Machine=rt::Model2AMachine;
using Effect=std::tuple<uint64_t,uint32_t,uint64_t>;
struct Step { uint32_t cycles=1; std::function<void(gen::Env &)> effect; };
struct Plan { std::map<uint32_t,Step> steps; std::vector<Effect> effects; };
Plan *active=nullptr;
bool compatible=true;
uint32_t code_word(uint32_t pc) { return 0x5c801610u ^ pc; }
constexpr uint32_t PRCB=0x200000, TABLE=0x200200, STACK=0x201800;
constexpr uint32_t ENABLE=0xe80004, ACK=0xe80000, TIMER=0xf00000;
constexpr uint64_t FRAME_BEGIN=rt::Model2AScreen::FRAME_NS/40;
constexpr uint64_t FRAME_END=(rt::Model2AScreen::FRAME_NS+rt::Model2AScreen::VBLANK_NS)/40;
struct Write { uint32_t address,data,mask;uint64_t cycles;bool operator==(const Write&)const=default; };
// Shadow only synthetic test RAM. Real bus synchronization/control/timers run
// unchanged. Direct map inspection avoids advancing the clock during asserts.
struct RamSpy final:rt::DeviceModel {
    rt::Cpu *cpu=nullptr;
    std::map<uint32_t,uint32_t> words;
    std::vector<Write> writes;
    bool inject_entry_edges=false, injected=false, throw_entry=false;
    bool claims(uint32_t a)const override{return a>=0x200000&&a<0x220000;}
    Check read(uint32_t a,uint32_t,uint32_t&v)override{v=words[a];return Strict;}
    void write(uint32_t a,uint32_t d,uint32_t m)override{
        words[a]=(words[a]&~m)|(d&m);writes.push_back({a,d,m,cpu->reference_cycles});
        if(throw_entry&&a==STACK+64-16)throw rt::Fatal("synthetic IRQ entry save failure");
        if(inject_entry_edges&&!injected&&a==STACK+64-16){
            injected=true;
            // Clear/reassert timer IRQ through authentic control/queue paths.
            // The next interrupt state-save bus access delivers the due timer.
            cpu->bus->write_dword(ACK,~4u);
            cpu->bus->write_dword(TIMER,0);
        }
    }
    void measured_mismatch(uint32_t,uint32_t,uint32_t)override{assert(false);}
    uint16_t flags(uint32_t)const override{return rt::Cpu::BURST;}
};
Machine::Images images(const Plan&p,bool screen,uint32_t corrupt=0){
    Machine::Images i;i.program.resize(0x600);
    auto put=[&](uint32_t a,uint32_t w){for(unsigned b=0;b<4;++b)i.program[a+b]=uint8_t(w>>(8*b));};
    put(4,PRCB);put(12,0x420);
    for(auto &[pc,_]:p.steps)put(pc,code_word(pc));
    if(corrupt)i.program[corrupt]^=1;
    if(screen){i.tgp_tables.resize(0x40000);i.copro_data.resize(0x800000);i.polygons.resize(4);i.textures.resize(2);}
    return i;
}
Machine::Options options(bool hook,bool screen){
    Machine::Options o;o.tgp=o.video=o.screen=screen;
    o.io=o.comm=o.uart=o.sound=o.external_video=false;o.control=true;
    o.instruction_boundary_hook=hook;return o;
}
struct Board {
    Plan plan;RamSpy ram;Machine machine;
    Board(Plan p,bool hook,bool screen=false,uint32_t corrupt=0):plan(std::move(p)),machine(images(plan,screen,corrupt),options(hook,screen)){
        auto&c=const_cast<rt::Cpu&>(machine.cpu());ram.cpu=&c;
        dynamic_cast<rt::Model2ABus*>(c.bus)->add_model(&ram);
        c.m_PRCB=PRCB;c.m_PC=0;c.m_ICR=0xf8f8f8f8;
        c.m_r[rt::I960_FP]=0x200800;c.m_r[rt::I960_SP]=0x200840;
        c.bus->write_dword(PRCB+20,TABLE);c.bus->write_dword(PRCB+24,STACK);
        ram.writes.clear();
    }
    rt::Cpu&cpu(){return const_cast<rt::Cpu&>(machine.cpu());}
    void vector(uint32_t handler,unsigned v=0xf8){cpu().bus->write_dword(TABLE+36+(v-8)*4,handler);ram.writes.clear();}
    void timer(uint32_t ticks){
        cpu().bus->write_dword(ENABLE,4);
        cpu().reference_cycles=2;cpu().bus->read_dword(ENABLE); // exact80ns delayed mask.
        cpu().bus->write_dword(TIMER,ticks);ram.writes.clear();
    }
    Machine::RunResult run(uint64_t n,bool frame=false){active=&plan;return frame?machine.run_until_frame(n):machine.run_for(n);}
};
void equal(const Board&a,const Board&b){
    auto x=a.machine.stats(),y=b.machine.stats();
    assert(x.instructions==y.instructions&&x.reference_cycles==y.reference_cycles&&x.board_time_ns==y.board_time_ns);
    assert(x.pc==y.pc&&x.previous_dispatch==y.previous_dispatch&&x.irq_entries==y.irq_entries);
    assert(x.frames_rendered==y.frames_rendered&&x.frames_parsed==y.frames_parsed&&x.screen_frame==y.screen_frame&&x.latched_frame==y.latched_frame);
    auto&c=a.machine.cpu();auto&d=b.machine.cpu();
    assert(!std::memcmp(c.m_r,d.m_r,sizeof(c.m_r))&&!std::memcmp(c.m_rcache,d.m_rcache,sizeof(c.m_rcache)));
    assert(!std::memcmp(c.m_rcache_frame_addr,d.m_rcache_frame_addr,sizeof(c.m_rcache_frame_addr)));
    assert(!std::memcmp(c.m_fp,d.m_fp,sizeof(c.m_fp))&&!std::memcmp(c.m_irq_line_state,d.m_irq_line_state,sizeof(c.m_irq_line_state)));
    assert(c.m_IP==d.m_IP&&c.m_PIP==d.m_PIP&&c.m_PC==d.m_PC&&c.m_AC==d.m_AC&&c.m_ICR==d.m_ICR);
    assert(c.m_rcache_pos==d.m_rcache_pos&&c.m_immediate_irq==d.m_immediate_irq&&c.m_immediate_vector==d.m_immediate_vector&&c.m_immediate_pri==d.m_immediate_pri);
    assert(a.ram.words==b.ram.words&&a.ram.writes==b.ram.writes&&a.plan.effects==b.plan.effects);
}
void result_equal(const Machine::RunResult&a,const Machine::RunResult&b){
    assert(a.reason==b.reason&&a.pc==b.pc&&a.instructions==b.instructions&&a.message==b.message);
    assert(a.address==b.address&&a.value==b.value&&a.width==b.width&&a.write==b.write);
}
void run_pair(Board&a,Board&b,uint64_t n,bool frame=false){result_equal(a.run(n,frame),b.run(n,frame));equal(a,b);}
Plan linear(unsigned count=8){Plan p;for(unsigned n=0;n<count;++n)p.steps.emplace(0x420+n*4,Step{});return p;}
void budgets_missing_guard_exceptions(){
    auto p=linear();Board a(p,false),b(p,true);run_pair(a,b,0);run_pair(a,b,3);run_pair(a,b,2);run_pair(a,b,0);run_pair(a,b,100);run_pair(a,b,1);
    assert(a.machine.stats().previous_dispatch==0x43c&&a.machine.stats().pc==0x440);
    Board c(p,false,false,0x428),d(p,true,false,0x428);run_pair(c,d,100);
    assert(c.machine.stats().instructions==2&&c.machine.stats().reference_cycles==2&&c.machine.stats().previous_dispatch==0x428);
    p.steps[0x428].effect=[](gen::Env&e){e.c.bus->write_word(0xdead0000,0x1234);};
    Board e(p,false),f(p,true);run_pair(e,f,100);assert(e.machine.stats().instructions==2&&e.machine.stats().reference_cycles==3);
    p.steps[0x428].effect=[](gen::Env&){throw rt::Fatal("synthetic effect failure");};
    Board g(p,false),h(p,true);run_pair(g,h,100);assert(g.machine.stats().previous_dispatch==0x428);
    p=linear();p.steps[0x420].cycles=2;Board i(p,false),j(p,true);
    i.cpu().reference_cycles=j.cpu().reference_cycles=UINT64_MAX/40-1;
    run_pair(i,j,2);assert(i.machine.stats().instructions==1&&i.machine.stats().previous_dispatch==0x420);
}
void same_cross_chunk_irqs_and_entry_edges(){
    for(uint32_t handler:{0x430u,0x500u})for(bool inject:{false,true}){
        auto p=linear();p.steps[handler]=Step{};p.steps[handler+4]=Step{};p.steps[handler+8]=Step{};
        Board a(p,false),b(p,true);a.vector(handler);b.vector(handler);a.timer(1);b.timer(1);
        a.ram.inject_entry_edges=b.ram.inject_entry_edges=inject;
        run_pair(a,b,3);assert(a.machine.stats().irq_entries==(inject?2u:1u));
        assert(std::get<1>(a.plan.effects.at(0))==0x420);
        assert(std::get<1>(a.plan.effects.at(1))==handler);
        assert(std::get<1>(a.plan.effects.at(2))==(inject?handler:handler+4));
    }
    // Preparation must precede has_code: IRQ redirects an otherwise absent PC.
    auto p=linear();p.steps[0x500]=Step{};Board a(p,false),b(p,true);a.vector(0x500);b.vector(0x500);a.timer(0);b.timer(0);
    a.cpu().m_IP=b.cpu().m_IP=0xdeadbeef;run_pair(a,b,1);assert(a.machine.stats().irq_entries==1&&a.machine.stats().previous_dispatch==0x500);
    p=linear();p.steps[0x500]=Step{};Board c(p,false),d(p,true);c.vector(0x500);d.vector(0x500);c.timer(1);d.timer(1);
    c.ram.throw_entry=d.ram.throw_entry=true;run_pair(c,d,3);
    assert(c.machine.stats().instructions==1&&c.machine.stats().previous_dispatch==0x420);
    // A missing IRQ handler is a frontier, never an attempted instruction.
    p=linear();Board e(p,false),f(p,true);e.vector(0x580);f.vector(0x580);e.timer(1);f.timer(1);
    run_pair(e,f,3);assert(e.machine.stats().instructions==1&&e.machine.stats().pc==0x580);
    assert(e.machine.stats().previous_dispatch==0x420&&e.machine.stats().irq_entries==1);run_pair(e,f,1);
}
void ret_pending_irq(){
    auto p=linear();p.steps[0x500]={0,[](gen::Env&e){e.c.do_ret();}};p.steps[0x520]=Step{};
    Board a(p,false),b(p,true);
    for(Board*c:{&a,&b}){
        c->vector(0x500,0xf0);c->vector(0x520,0xe8);c->cpu().m_ICR=0x00e800f0;
        c->cpu().execute_set_input(0,1);c->cpu().check_immediate_irqs();
        c->cpu().execute_set_input(2,1); // lowerpriority pending-table path.
    }
    run_pair(a,b,1);assert(a.machine.stats().irq_entries==2&&a.machine.stats().pc==0x520);run_pair(a,b,1);
}
void frame_boundaries(){
    // MMIO advancement during instruction: Frame wins the exact-budget tie.
    auto p=linear();p.steps[0x420]={uint32_t(FRAME_END),[](gen::Env&e){(void)e.c.bus->read_dword(ACK);}};
    Board a(p,false,true),b(p,true,true);run_pair(a,b,1,true);assert(a.machine.stats().frames_rendered==1&&a.machine.stats().instructions==1);
    run_pair(a,b,0,true);run_pair(a,b,1,true);
    // No access after cycles: budget must not add an extra clock advance.
    p.steps[0x420]={uint32_t(FRAME_END),{}};Board c(p,false,true),d(p,true,true);run_pair(c,d,1,true);
    assert(c.machine.stats().frames_rendered==0);run_pair(c,d,1,true);assert(c.machine.stats().instructions==1&&c.machine.stats().frames_rendered==1);
    // IRQentry crosses frame end at count0; resume must prepare at the samecount.
    p=linear();p.steps[0x500]=Step{};Board e(p,false,true),f(p,true,true);
    e.vector(0x500);f.vector(0x500);e.timer(uint32_t(FRAME_END-5));f.timer(uint32_t(FRAME_END-5));
    e.cpu().reference_cycles=f.cpu().reference_cycles=FRAME_END-3;
    e.ram.inject_entry_edges=f.ram.inject_entry_edges=true;
    run_pair(e,f,2,true);assert(e.machine.stats().instructions==0&&e.machine.stats().frames_rendered==1);
    assert(e.machine.stats().previous_dispatch==0);run_pair(e,f,1);assert(e.machine.stats().irq_entries==2);
    // Effects still complete atomically if a frame then exception occurs.
    p=linear();p.steps[0x420]={uint32_t(FRAME_END),[](gen::Env&e){e.c.bus->write_word(0xdead0000,9);}};
    Board g(p,false,true),h(p,true,true);run_pair(g,h,1,true);assert(g.machine.stats().instructions==0&&g.machine.stats().frames_rendered==1);
}
}
namespace gen {
bool instruction_boundary_hook_compatible(){return compatible;}
bool has_code(uint32_t pc){return active&&active->steps.contains(pc);}
uint64_t native_instructions(){return active?active->steps.size():0;}
void run(Env&e){
    const uint32_t chunk=e.c.m_IP>>8;
    for(;;){
        if(e.ls.boundary_yielded()||!has_code(e.c.m_IP)||(e.c.m_IP>>8)!=chunk)return;
        const uint32_t pc=e.c.m_IP;
        // Match exact generated label order: setIP,boundary,liveguard,cost,effects,count.
        e.c.m_IP=pc;if(e.ls.boundary()){if(e.ls.finished())return;continue;}
        if(e.c.bus->read_dword(pc)!=code_word(pc))throw rt::Fatal("synthetic live guard differs");
        const Step step=active->steps.at(pc);e.c.consume_reference_cycles(step.cycles);
        if(step.effect)step.effect(e);
        active->effects.emplace_back(e.ls.count,pc,e.c.reference_cycles);
        if(e.c.m_IP==pc)e.c.m_IP=pc+4;
        ++e.ls.count;
    }
}
}
namespace rt::tgpgen {
extern const uint32_t program_crc32=0,program_words=0;
void run(Tgp&,uint64_t){throw TgpFatal("synthetic screen fixture must not execute TGP");}
}
int main(){
    budgets_missing_guard_exceptions();same_cross_chunk_irqs_and_entry_edges();ret_pending_irq();frame_boundaries();
    compatible=false;
    for(bool hook:{false,true}){bool rejected=false;try{Board bad(linear(),hook);}catch(const rt::Fatal&){rejected=true;}assert(rejected);}
    std::puts("Model2A old/new boundary: CPU/cache/IRQ/cycles/ordered writes/frame/frontier differential passed");
}
