// Pinned-source differential: synthetic public opcodes, no game program.
#include "m68000-timing-reference.h"
#include "m68000-native-fixture.h"
#include "m68k/timing.h"
#include <cstdlib>
#include <set>
struct Memory final:snd::AddressSpace {
    std::vector<uint8_t> bytes;
    std::vector<std::array<uint32_t,3>> writes;
    explicit Memory(const std::vector<uint8_t>& source):bytes(source){}
    uint16_t read16(uint32_t a,uint16_t mask)override{return uint16_t(bytes[a]<<8|bytes[a+1])&mask;}
    void write16(uint32_t a,uint16_t value,uint16_t mask)override{
        writes.push_back({a,uint32_t(value&mask),mask});
        value=uint16_t((read16(a,0xffff)&~mask)|(value&mask));bytes[a]=uint8_t(value>>8);bytes[a+1]=uint8_t(value);
    }
};
int main() {
    unsigned checks=0,errors=0,frontiers=0,odd=0,zero_divides=0;
    std::set<unsigned> failed;
    for(unsigned op:reference_opcodes)for(unsigned iteration=0;iteration<8;++iteration) {
        MicroOracle reference;
        for(unsigned r=0;r<8;++r)reference.m_da[r]=iteration==7?0xffff:iteration*2+r*8;
        for(unsigned r=8;r<17;++r)reference.m_da[r]=0x2000+(r-8)*0x100;
        reference.m_sr=0x2700|(iteration*7&15);
        for(unsigned a=0;a<0x80000;a+=2)reference.word(a,uint16_t(a*31+iteration*71));
        for(unsigned offset=2;offset<20;offset+=2)reference.word(0x600100+offset,0x0010);
        reference.setup(op);
        m68k::Insn in;
        if(!m68k::decode(0x600100,[&](uint32_t a)->std::optional<uint16_t>{return uint16_t(reference.memory[a]<<8|reference.memory[a+1]);},in)
            || !m68k::timing_plan(in))continue;
        if(in.op==m68k::Op::Rts) {reference.word(reference.m_da[16],0x0060);reference.word(reference.m_da[16]+2,0x0200);}
        if(in.op==m68k::Op::Rte || in.op==m68k::Op::Rtr) {
            reference.word(reference.m_da[16],0x2700);reference.word(reference.m_da[16]+2,0x0060);reference.word(reference.m_da[16]+4,0x0200);
        }
        Memory memory(reference.memory);snd::Cpu68k native;native.bus=&memory;
        native.pc=0x600100;native.set_sr(reference.m_sr);native.model2a_timing=true;
        for(unsigned r=0;r<8;++r){native.d[r]=reference.m_da[r];native.a[r]=reference.m_da[r==7?16:r+8];}
        native.other_sp=reference.m_da[15];
        reference.waits=true;reference.run(op);
        if(reference.m_inst_state==MicroOracle::S_ADDRESS_ERROR){++odd;continue;}
        try {if(!native_run(op,native))continue;}
        catch(const rt::Fatal &error) {
            if(std::string(error.what()).find("timed prefetch outside")!=std::string::npos){++frontiers;continue;}
            if((in.op==m68k::Op::Divu || in.op==m68k::Op::Divs) && std::string(error.what()).find("divide by zero")!=std::string::npos){++zero_divides;continue;}
            std::fprintf(stderr,"unexpected frontier %04x: %s\n",op,error.what());return 2;
        }
        unsigned data=0,prefetch=0,waits=0;
        for(const auto &access:reference.accesses){
            if(access.kind=='p')++prefetch;else ++data;
            if(access.address<0x80000 || (access.address>=0x100000 && access.address<0x101000))++waits;
        }
        std::vector<std::array<uint32_t,3>> writes;
        for(const auto &access:reference.accesses)if(access.kind=='w')
            writes.push_back({access.address,uint32_t(access.data&access.mask),access.mask});
        ++checks;
        const auto cycles=100000000-reference.m_icount;
        if(native.cycles!=uint64_t(cycles) || native.cycle_stats.data_transfers+native.cycle_stats.prefetch_transfers!=data+prefetch || native.cycle_stats.mapped_waits!=waits || memory.writes!=writes) {
            failed.insert(op);
            if(errors++<100)std::printf("%04x %s state%u native cycles%llu/data%llu/fetch%llu/wait%llu reference %d/%u/%u/%u writes_equal%d\n",
                op,m68k::format_mame(in).c_str(),iteration,(unsigned long long)native.cycles,
                (unsigned long long)native.cycle_stats.data_transfers,(unsigned long long)native.cycle_stats.prefetch_transfers,
                (unsigned long long)native.cycle_stats.mapped_waits,cycles,data,prefetch,waits,int(memory.writes==writes));
        }
    }
    unsigned irq_checks=0;
    for(unsigned phase=0;phase<20;++phase)for(unsigned level=1;level<=7;++level)for(bool ram_handler:{false,true}) {
        MicroOracle reference;reference.waits=true;reference.cycle_origin=phase;
        reference.m_next_state=(level<<24)|MicroOracle::S_INTERRUPT;reference.m_da[16]=0x5000;
        reference.m_pc=0x600104;reference.m_sr=0x2015; // internal prefetch PC is architectural PC+2
        reference.word((24+level)*4,ram_handler?0:0x0060);reference.word((24+level)*4+2,0x0200);
        Memory memory(reference.memory);snd::Cpu68k native;native.bus=&memory;native.model2a_timing=true;
        native.cycles=phase;native.pc=0x600102;native.a[7]=0x5000;native.set_sr(0x2015);
        reference.state_interrupt();native.take_irq(level);
        const auto expected=phase+100000000-reference.m_icount;
        std::vector<std::array<uint32_t,3>> writes;
        for(const auto &access:reference.accesses)if(access.kind=='w')writes.push_back({access.address,uint32_t(access.data&access.mask),access.mask});
        ++irq_checks;
        if(native.cycles!=uint64_t(expected) || native.pc!=reference.m_pc-2 || memory.writes!=writes) {
            ++errors;std::printf("IRQ mismatch phase%u level%u RAM%d native%llu reference%u pc%06x/%06x\n",phase,level,int(ram_handler),
                (unsigned long long)native.cycles,expected,native.pc,reference.m_pc-2);
        }
    }
    for(bool ram_handler:{false,true}) {
        MicroOracle reference;reference.waits=true;reference.word(0,0);reference.word(2,0x5000);
        reference.word(4,ram_handler?0:0x60);reference.word(6,0x100);
        Memory memory(reference.memory);snd::Cpu68k native;native.bus=&memory;native.model2a_timing=true;
        reference.state_reset();native.reset();++irq_checks;
        if(native.cycles!=uint64_t(100000000-reference.m_icount) || native.pc!=reference.m_pc-2 || native.a[7]!=reference.m_da[16])++errors;
    }
    std::printf("M68000 interrupt/reset reference: %u checks\n",irq_checks);
    std::printf("M68000 explicit zero-divide exception frontiers: %u\n",zero_divides);
    std::printf("M68000 reference: %u instruction checks, %u errors, %u explicit prefetch frontiers, %u odd-address exceptions excluded\n",checks,errors,frontiers,odd);
    for(auto op:failed)std::printf("failed opcode %04x\n",op);
    return errors?1:0;
}
