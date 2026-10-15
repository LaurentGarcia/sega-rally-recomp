// Pinned-source differential: synthetic public opcodes, no game program.
// Completed supported cases compare architectural D/A registers, both stack
// banks, PC, defined SR bits, and the entire synthetic 24-bit memory image.
// This does not model a value-bearing native prefetch queue or SCSP/MMIO bus
// phases. The reference's internal prefetch PC is architectural PC + 2, as in
// the existing interrupt and separate sound-phase fixtures; no bus clocks or
// register/memory values are adjusted to make the comparison pass.
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
namespace {
struct StateComparison {
    bool registers=true,pc=true,status=true,memory=true;
    bool equal()const{return registers && pc && status && memory;}
};
StateComparison architectural_state(const snd::Cpu68k &native,const MicroOracle &reference,
                                     const Memory &memory,uint16_t sr_mask=0xffff) {
    if(reference.m_sp!=15 && reference.m_sp!=16)
        throw std::runtime_error("reference selected an invalid architectural stack bank");
    StateComparison result;
    for(unsigned r=0;r<8;++r) {
        result.registers &= native.d[r]==reference.m_da[r];
        result.registers &= native.a[r]==reference.m_da[r==7?reference.m_sp:r+8];
    }
    result.registers &= native.other_sp==reference.m_da[reference.m_sp==16?15:16];
    result.pc=native.pc==reference.m_pc-2;
    result.status=(native.sr()&sr_mask)==(reference.m_sr&sr_mask);
    // Includes all Model2A RAM, memory-shaped SCSP addresses, stacks, program
    // and otherwise untouched bytes. These are flat synthetic bytes, not a
    // claim about peripheral register semantics or live sound-chip RAM.
    result.memory=memory.bytes==reference.memory;
    return result;
}
void print_state(const snd::Cpu68k &native,const MicroOracle &reference,const Memory &memory,uint16_t sr_mask) {
    for(unsigned r=0;r<8;++r) {
        if(native.d[r]!=reference.m_da[r]) std::printf(" D%u=%08x/%08x",r,native.d[r],reference.m_da[r]);
        const auto expected=reference.m_da[r==7?reference.m_sp:r+8];
        if(native.a[r]!=expected) std::printf(" A%u=%08x/%08x",r,native.a[r],expected);
    }
    const auto other=reference.m_da[reference.m_sp==16?15:16];
    if(native.other_sp!=other) std::printf(" otherSP=%08x/%08x",native.other_sp,other);
    if(native.pc!=reference.m_pc-2) std::printf(" PC=%08x/%08x",native.pc,reference.m_pc-2);
    if((native.sr()&sr_mask)!=(reference.m_sr&sr_mask)) std::printf(" SR=%04x/%04x mask=%04x",native.sr(),reference.m_sr,sr_mask);
    const auto mismatch=std::mismatch(memory.bytes.begin(),memory.bytes.end(),reference.memory.begin(),reference.memory.end());
    if(mismatch.first!=memory.bytes.end()) std::printf(" memory[%06zx]=%02x/%02x",size_t(mismatch.first-memory.bytes.begin()),unsigned(*mismatch.first),unsigned(*mismatch.second));
    std::puts("");
}
}
int main() {
    unsigned checks=0,errors=0,frontiers=0,odd=0,zero_divides=0;
    unsigned undecoded=0,unsupported=0,unemitted=0,undefined_div_flags=0;
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
        if(!m68k::decode(0x600100,[&](uint32_t a)->std::optional<uint16_t>{return uint16_t(reference.memory[a]<<8|reference.memory[a+1]);},in)) {++undecoded;continue;}
        if(!m68k::timing_plan(in)) {++unsupported;continue;}
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
        try {if(!native_run(op,native)) {++unemitted;continue;}}
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
        // DIVU/DIVS overflow leaves N/Z architecturally undefined. As in the
        // existing exhaustive division fixture, compare every other SR bit,
        // count this limitation explicitly, and never rewrite either SR.
        const bool undefined_nz=(in.op==m68k::Op::Divu || in.op==m68k::Op::Divs) && (reference.m_sr&2);
        const uint16_t sr_mask=undefined_nz?uint16_t(0xfff3):uint16_t(0xffff);
        undefined_div_flags+=undefined_nz;
        const auto state=architectural_state(native,reference,memory,sr_mask);
        if(native.cycles!=uint64_t(cycles) || native.cycle_stats.data_transfers+native.cycle_stats.prefetch_transfers!=data+prefetch || native.cycle_stats.mapped_waits!=waits || memory.writes!=writes || !state.equal()) {
            failed.insert(op);
            if(errors++<100) {
                std::printf("%04x %s state%u native cycles%llu/data%llu/fetch%llu/wait%llu reference %d/%u/%u/%u writes_equal%d registers_equal%d pc_equal%d defined_sr_equal%d memory_equal%d\n",
                op,m68k::format_mame(in).c_str(),iteration,(unsigned long long)native.cycles,
                (unsigned long long)native.cycle_stats.data_transfers,(unsigned long long)native.cycle_stats.prefetch_transfers,
                (unsigned long long)native.cycle_stats.mapped_waits,cycles,data,prefetch,waits,int(memory.writes==writes),
                int(state.registers),int(state.pc),int(state.status),int(state.memory));
                if(!state.equal()) print_state(native,reference,memory,sr_mask);
            }
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
        const auto state=architectural_state(native,reference,memory);
        if(native.cycles!=uint64_t(expected) || native.pc!=reference.m_pc-2 || memory.writes!=writes || !state.equal()) {
            ++errors;std::printf("IRQ mismatch phase%u level%u RAM%d native%llu reference%u pc%06x/%06x state_equal%d\n",phase,level,int(ram_handler),
                (unsigned long long)native.cycles,expected,native.pc,reference.m_pc-2,int(state.equal()));
            if(!state.equal()) print_state(native,reference,memory,0xffff);
        }
    }
    for(bool ram_handler:{false,true}) {
        MicroOracle reference;reference.waits=true;reference.word(0,0);reference.word(2,0x5000);
        reference.word(4,ram_handler?0:0x60);reference.word(6,0x100);
        Memory memory(reference.memory);snd::Cpu68k native;native.bus=&memory;native.model2a_timing=true;
        reference.state_reset();native.reset();++irq_checks;
        // RESET leaves CCR and the other general registers architecturally
        // undefined. Check defined system SR/SP/PC and untouched memory only;
        // do not equate undefined reset values with hardware behavior.
        if(native.cycles!=uint64_t(100000000-reference.m_icount) || native.pc!=reference.m_pc-2 || native.a[7]!=reference.m_da[16] ||
           (native.sr()&0xffe0)!=(reference.m_sr&0xffe0) || memory.bytes!=reference.memory) {
            ++errors;std::printf("RESET mismatch RAM%d native SR%04x/PC%06x/SP%08x reference SR%04x/PC%06x/SP%08x memory_equal%d\n",int(ram_handler),
                native.sr(),native.pc,native.a[7],reference.m_sr,reference.m_pc-2,reference.m_da[16],int(memory.bytes==reference.memory));
        }
    }
    std::printf("M68000 interrupt/reset reference: %u checks\n",irq_checks);
    std::printf("M68000 explicit zero-divide exception frontiers: %u\n",zero_divides);
    std::printf("M68000 explicit DIV overflow undefined N/Z exclusions: %u; RESET undefined CCR/general registers excluded in 2 reset cases\n",undefined_div_flags);
    std::printf("M68000 unsupported timing cases: %u; undecoded: %u; unemitted: %u (not semantic comparisons)\n",unsupported,undecoded,unemitted);
    std::printf("M68000 architectural state and complete synthetic 24-bit memory comparisons: %u instruction cases + 280 IRQ cases; no native prefetch-value/bus-phase/SCSP/full-board claim\n",checks);
    std::printf("M68000 reference: %u instruction checks, %u errors, %u explicit prefetch frontiers, %u odd-address exceptions excluded\n",checks,errors,frontiers,odd);
    for(auto op:failed)std::printf("failed opcode %04x\n",op);
    return errors?1:0;
}
