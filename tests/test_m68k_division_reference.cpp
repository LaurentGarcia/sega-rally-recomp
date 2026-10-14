// Independent pinned M68000 microcode versus production-emitted division.
// Synthetic operands only; zero-divide exception delivery remains unsupported.
#include "m68000-timing-reference.h"
#include "m68000-native-fixture.h"
#include <cstdio>

int main() {
    MicroOracle reference;
    snd::Cpu68k native;
    native.model2a_timing=true;
    uint64_t checks=0;
    unsigned errors=0;
    auto check=[&](uint32_t dividend,uint16_t divisor) {
        if(!divisor)return;
        for(bool sign:{false,true}) {
            const auto opcode=uint16_t(sign?0x81c1:0x80c1);
            reference.m_sr=0x2700;
            reference.m_da[0]=dividend;reference.m_da[1]=divisor;
            reference.setup(opcode);reference.run(opcode);
            native.pc=0x600100;native.set_sr(0x2700);native.cycles=0;native.cycle_stats={};
            native.d[0]=dividend;native.d[1]=divisor;
            if(!native_run(opcode,native))throw std::runtime_error("production division fixture missing");
            const auto expected=uint64_t(100000000-reference.m_icount);
            // Overflow N/Z are undefined by the ISA and are not compared.
            const uint16_t flags=(reference.m_sr&2)?uint16_t(0x13):uint16_t(0x1f);
            ++checks;
            if(native.cycles!=expected || native.d[0]!=reference.m_da[0]
                    || (native.sr()&flags)!=(reference.m_sr&flags)) {
                if(errors++<30)std::printf("%s %08x / %04x cycles %llu/%llu result %08x/%08x flags %04x/%04x\n",
                    sign?"DIVS":"DIVU",dividend,divisor,(unsigned long long)native.cycles,
                    (unsigned long long)expected,native.d[0],reference.m_da[0],native.sr(),reference.m_sr);
            }
        }
    };
    for(unsigned divisor=1;divisor<65536;++divisor)
        for(uint32_t dividend:{0u,1u,0x7fffu,0x8000u,0xffffu,0x10000u,0x7fff0000u,0x80000000u,0xffffffffu})
            check(dividend,uint16_t(divisor));
    uint32_t random=0x9d14abd7;
    for(unsigned i=0;i<100000;++i) {
        random=random*1664525+1013904223;const auto dividend=random;
        random=random*1664525+1013904223;check(dividend,uint16_t(random>>8));
    }
    std::printf("M68000 division reference: %llu emitted instruction checks, %u errors\n",(unsigned long long)checks,errors);
    return errors?1:0;
}
