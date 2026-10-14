// Synthetic instruction fixtures only. Reference interpreter is test-only.
#include "i960/timing.h"
#include "refcore/i960_ref.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

struct Memory : rt::Bus {
    std::array<uint8_t,0x10000> bytes{};
    bool burst=true;
    uint32_t fetch(uint32_t a) override { return read_dword(a); }
    uint8_t read_byte(uint32_t a) override { return bytes.at(a); }
    uint16_t read_word(uint32_t a) override { return read_byte(a)|(uint16_t(read_byte(a+1))<<8); }
    uint32_t read_dword(uint32_t a) override { return read_word(a)|(uint32_t(read_word(a+2))<<16); }
    void write_byte(uint32_t a,uint8_t v) override { bytes.at(a)=v; }
    void write_word(uint32_t a,uint16_t v) override { write_byte(a,v);write_byte(a+1,v>>8); }
    void write_dword(uint32_t a,uint32_t v) override { write_word(a,v);write_word(a+2,v>>16); }
    uint16_t flags(uint32_t) override { return burst?rt::Cpu::BURST:0; }
};

static uint32_t word(const i960::OpInfo& op) {
    if(op.fmt==i960::Format::Reg) {
        uint32_t result=(uint32_t(op.code>>4)<<24)|((op.code&15)<<7)|16|(18<<14)|(24<<19);
        if(op.pat==10 || op.pat==20 || op.pat==-20 || op.pat==-30)
            result=(uint32_t(op.code>>4)<<24)|((op.code&15)<<7)|(1<<14)|(2<<19)|0x3800;
        if(op.code>=0x6c0 && op.code<=0x6c3) result&=~0x2000u; // integer destination
        return result;
    }
    if(op.fmt==i960::Format::Mem) return (uint32_t(op.code)<<24)|(24<<19)|0x100;
    return uint32_t(op.code)<<24;
}

static unsigned compare(const i960::OpInfo& op) {
    const uint32_t opcode=word(op);
    const auto decoded=i960::decode(0x1000,opcode,0);
    const auto expected=i960::reference_opcode_cycles(decoded);
    assert(expected.has_value());
    for(unsigned scenario=0;scenario<4;scenario++) {
        Memory mem;mem.burst=!(scenario&1);
        mem.write_dword(0x1000,opcode);
        rt::I960Ref ref(&mem);
        ref.m_IP=0x1000;ref.m_AC=(scenario&2)?7:0;
        ref.m_r[16]=4;ref.m_r[18]=8;ref.m_r[31]=0x2000;ref.m_r[1]=0x2040;
        ref.m_r[0]=0x2000;ref.m_rcache_pos=1;ref.m_rcache[0][2]=0x1000;
        ref.m_fp[0]=2.5;ref.m_fp[1]=1.25;
        ref.m_rcache_frame_addr[0]=0x4000;
        ref.m_icount=0;
        try { ref.execute_one(); }
        catch(const rt::Fatal&) {
            // A taken conditional fault is an explicit unsupported fault;
            // its reference deducts one cycle before raising fatalerror.
            assert(op.flow==i960::Flow::CondFault);
        }
        if(unsigned(-ref.m_icount)!=*expected) {
            std::fprintf(stderr,"Timing mismatch %s code=%03x scenario=%u reference=%d metadata=%u\n",
                op.mnem,op.code,scenario,-ref.m_icount,*expected);
            assert(false);
        }
    }
    return 4;
}

int main() {
    unsigned opcodes=0,checks=0;
    for(unsigned major=0;major<256;major++) {
        const auto* op=i960::lookup_major(major);
        if(op && op->mame_exec) { checks+=compare(*op);opcodes++; }
    }
    for(unsigned code=0;code<4096;code++) {
        const auto* op=i960::lookup_reg(code);
        if(op && op->mame_exec) { checks+=compare(*op);opcodes++; }
    }
    assert(!i960::reference_opcode_cycles(i960::decode(0,0,0)));
    assert(!i960::reference_opcode_cycles(i960::decode(0,0x3f000000,0)));
    assert(!i960::reference_opcode_cycles(i960::decode(0,0x90801800,0))); // reserved MEMB mode
    // Long instruction displacement, addressing form and alignment do not
    // change pinned deductions. Include IP-relative and absolute MEMB forms.
    for(auto opcode:{0x90801400u,0x90803000u,0x98801400u,0xb0803000u}) {
        auto instruction=i960::decode(0x1000,opcode,0x100);
        auto cost=i960::reference_opcode_cycles(instruction);
        assert(cost && instruction.length==8);
        Memory mem;mem.write_dword(0x1000,opcode);mem.write_dword(0x1004,0x100);
        rt::I960Ref ref(&mem);ref.m_IP=0x1000;ref.execute_one();
        assert(unsigned(-ref.m_icount)==*cost);checks++;
    }
    for(auto opcode:{0x09000000u,0x0a000000u,0x86000100u,
                     (0x66u<<24)|(0u<<7), (0x66u<<24)|(0xdu<<7)})
        assert(i960::reference_opcode_cycles(i960::decode(0x1000,opcode,0))==0);
    std::printf("Pinned MAME timing: %u opcodes, %u oracle comparisons; helper zero-cost/unknown contracts passed\n",opcodes,checks);
}
