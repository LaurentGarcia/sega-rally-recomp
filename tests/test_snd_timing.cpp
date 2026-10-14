// Model2A bounded sound timing: source-backed totals and explicit boundaries.
#include "runtime/snd_sched.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <limits>
namespace {
unsigned checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("sound timing check "+std::to_string(checks));}
template<class F>void fatal(F fn){bool caught=false;try{fn();}catch(const rt::Fatal&){caught=true;}check(caught);}
struct Bus:snd::AddressSpace {
    struct Access{bool write;uint32_t address;uint16_t value,mask;};
    std::vector<uint8_t> memory=std::vector<uint8_t>(0x1000000);
    std::vector<Access> accesses;
    uint16_t read16(uint32_t a,uint16_t mask)override{
        const uint16_t value=uint16_t(memory[a]<<8|memory[a+1]);accesses.push_back({false,a,value,mask});return value&mask;
    }
    void write16(uint32_t a,uint16_t value,uint16_t mask)override{
        accesses.push_back({true,a,value,mask});const uint16_t old=uint16_t(memory[a]<<8|memory[a+1]);
        value=uint16_t((old&~mask)|(value&mask));memory[a]=uint8_t(value>>8);memory[a+1]=uint8_t(value);
    }
    void put32(uint32_t a,uint32_t value){for(int i=3;i>=0;--i){memory[a+i]=uint8_t(value);value>>=8;}}
};
void reset_and_waits(){
    Bus bus;bus.put32(0,0x5000);bus.put32(4,0x600100);
    snd::Cpu68k c;c.bus=&bus;c.model2a_timing=true;
    uint64_t callback=0;c.on_cycles=[&](uint64_t cycles){check(!c.timing_active());callback=cycles;};
    c.reset();check(c.pc==0x600100 && c.a[7]==0x5000 && c.sr()==0x2700);
    check(c.cycles==38 && callback==38);check(c.cycle_stats.data_transfers==4);
    check(c.cycle_stats.prefetch_transfers==2 && c.cycle_stats.mapped_waits==4);
    // Live code guards and inspection are not data bus cycles.
    c.r16(0x600100);check(c.cycle_stats.data_transfers==4);
    c.begin_instruction();c.r8(0);c.r8(0x7ffff);c.w16(0x100ffe,0x1234);
    c.r16(0x600000);c.w16(0x400000,0);c.account_prefetch(0x7fffe);c.finish_instruction(24);
    check(c.cycles==66 && c.cycle_stats.mapped_waits==8 && c.cycle_stats.data_transfers==9);
    check(snd::Cpu68k::model2a_wait(0x80000)==0 && snd::Cpu68k::model2a_wait(0x101000)==0);
    check(snd::Cpu68k::model2a_wait(0x1100000)==1);
    c.reset();check(c.cycles==104); // monotonic reset, not clock rewind
}
void interrupt_phases(){
    constexpr unsigned expected[]={59,58,57,56,55,54,63,62,61,60};
    for(unsigned phase=0;phase<10;++phase){
        Bus bus;bus.put32(27*4,0x600200);snd::Cpu68k c;c.bus=&bus;c.model2a_timing=true;
        c.pc=0x600102;c.a[7]=0x5000;c.set_sr(0x2015);c.cycles=phase;
        c.on_irq_ack=[&](int level){check(level==3);check(bus.accesses.size()==1);
            check(bus.accesses[0].address==0x4ffe && bus.accesses[0].value==0x0102);};
        c.take_irq(3);check(c.cycles==phase+expected[phase]);check(c.pc==0x600200 && c.a[7]==0x4ffa && c.sr()==0x2315);
        check(c.cycle_stats.data_transfers==5 && c.cycle_stats.prefetch_transfers==2);
        check(c.cycle_stats.mapped_waits==5 && c.cycle_stats.iack_transfers==1);
        check(c.cycle_stats.iack_waits==expected[phase]-49);
        check(bus.accesses.size()==5 && bus.accesses[1].address==0x4ffa && bus.accesses[2].address==0x4ffc);
    }
    Bus bus;bus.put32(31*4,0x600200);snd::Cpu68k c;c.bus=&bus;c.model2a_timing=true;
    c.a[7]=0x6000;c.other_sp=0x5000;c.sys=0x0700;c.nmi_pending=true;
    c.take_irq(7);check(!c.nmi_pending && c.a[7]==0x4ffa && c.other_sp==0x6000);
}
void guards_and_limits(){
    Bus bus;bus.put32(0,0x5000);bus.put32(4,0x600100);snd::Cpu68k c;c.bus=&bus;
    fatal([&]{c.begin_instruction();});c.model2a_timing=true;
    fatal([&]{c.finish_instruction(4);});fatal([&]{c.account_prefetch(0);});fatal([&]{c.dummy_read(0,2);});
    c.sys=0xa700;fatal([&]{c.begin_instruction();});c.sys=0x2700;
    c.begin_instruction();const auto before_sr=c.sys;const auto before_sp=c.a[7];
    fatal([&]{c.reset();});fatal([&]{c.take_irq(3);});fatal([&]{c.begin_instruction();});
    check(c.sys==before_sr && c.a[7]==before_sp && bus.accesses.empty());
    fatal([&]{c.account_prefetch(0x800000);});fatal([&]{c.account_prefetch(1);});fatal([&]{c.dummy_read(0,3);});
    c.finish_instruction(4);
    c.on_cycles=[&](uint64_t){fatal([&]{c.begin_instruction();});fatal([&]{c.reset();});fatal([&]{c.take_irq(2);});};
    c.begin_instruction();c.finish_instruction(4);check(c.cycles==8);
    c.on_cycles=[](uint64_t){throw rt::Fatal("callback");};c.begin_instruction();fatal([&]{c.finish_instruction(4);});
    c.on_cycles={};c.begin_instruction();c.finish_instruction(4);check(c.cycles==16);
    c.cycles=UINT64_MAX-3;c.begin_instruction();fatal([&]{c.finish_instruction(4);});check(c.cycles==UINT64_MAX-3);
    snd::Cpu68k counters;counters.model2a_timing=true;counters.bus=&bus;
    counters.cycle_stats.prefetch_transfers=UINT64_MAX;
    counters.begin_instruction();counters.r16(0);counters.account_prefetch(0x600100);
    fatal([&]{counters.finish_instruction(4);});check(counters.cycles==0 && counters.cycle_stats.data_transfers==0);
    snd::Cpu68k scheduler_cpu;snd::Sched schedule(scheduler_cpu);schedule.auto_irq=false;
    schedule.count=UINT64_MAX;schedule.poke();check(schedule.next_count==UINT64_MAX);
    schedule.count=0;schedule.end_cycles=10;scheduler_cpu.cycles=9;check(!schedule.finished());
    scheduler_cpu.cycles=10;check(schedule.finished() && schedule.boundary());
}
void rational_time(){
    check(snd::Cpu68k::model2a_time_ns(0)==0);check(snd::Cpu68k::model2a_time_ns(1)==89);
    check(snd::Cpu68k::model2a_time_ns(11'289'600)==1'000'000'000);
    for(uint64_t cycles=0;cycles<100000;cycles+=97){
        const uint64_t expected=(cycles*1'000'000'000+11'289'599)/11'289'600;
        check(snd::Cpu68k::model2a_time_ns(cycles)==expected);
    }
    fatal([]{snd::Cpu68k::model2a_time_ns(UINT64_MAX);});
    const uint64_t second=UINT64_MAX/1'000'000'000;
    fatal([&]{snd::Cpu68k::model2a_time_ns(second*11'289'600+11'289'599);});
}
}
int main(){reset_and_waits();interrupt_phases();guards_and_limits();rational_time();
    fatal([]{snd::Cpu68k::divu_cycles(1,0);});fatal([]{snd::Cpu68k::divs_cycles(1,0);});
    std::printf("Sound timing: %u checks passed\n",checks);}
