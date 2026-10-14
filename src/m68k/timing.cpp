// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Derived and differentially checked against MAME's generated M68000
// microcode at dddd73680656e355bb2b5beecab1167c9f07bf81, not its separate
// Musashi backend. m68000gen.py charges four cycles per bus completion and
// two per internal nanocode step. The generator resolves addressing modes;
// native code supplies branch, shift, bit-number and multiply decisions.
#include "m68k/timing.h"
#include <bit>

namespace m68k {
namespace {
unsigned ea_read(const Ea &ea,unsigned size) {
    unsigned cost=0;
    switch(ea.mode) {
    case Mode::Dn:case Mode::An:return 0;
    case Mode::Ind:case Mode::PostInc:cost=4;break;
    case Mode::PreDec:cost=6;break;
    case Mode::Disp:case Mode::AbsW:case Mode::PcDisp:cost=8;break;
    case Mode::Index:case Mode::PcIndex:cost=10;break;
    case Mode::AbsL:cost=12;break;
    case Mode::Imm:cost=4;break;
    default:return 0;
    }
    return cost+(size==4?4:0);
}
unsigned control_ea(const Ea &ea) {
    switch(ea.mode) {
    case Mode::Ind:return 0;
    case Mode::Disp:case Mode::AbsW:case Mode::PcDisp:return 4;
    case Mode::Index:case Mode::PcIndex:return 8;
    case Mode::AbsL:return 8;
    default:return 0;
    }
}
}
std::optional<TimingPlan> timing_plan(const Insn &in) {
    TimingPlan result;
    auto cost=[&](unsigned value){result.cycles=uint16_t(value);};
    const bool longword=in.size==4;
    const bool memory=in.dst.is_mem();
    switch(in.op) {
    case Op::Move:case Op::Movea: {
        unsigned destination=ea_read(in.dst,in.size);
        if(in.dst.mode==Mode::PreDec)destination-=2;
        cost(4+ea_read(in.src,in.size)+destination);break;
    }
    case Op::Moveq:case Op::Swap:case Op::Ext:case Op::Nop:cost(4);break;
    case Op::Lea:cost(4+control_ea(in.src));break;
    case Op::Pea:cost(12+control_ea(in.src));break;
    case Op::Clr:case Op::Not:case Op::Neg:case Op::Negx:
        cost(memory?(longword?12:8)+ea_read(in.dst,in.size):(longword?6:4));
        result.dummy_destination_read=in.op==Op::Clr && memory;break;
    case Op::Tst:cost(4+ea_read(in.src,in.size));break;
    case Op::And:case Op::Or:case Op::Add:case Op::Sub:
        cost(memory?(longword?12:8)+ea_read(in.dst,in.size):
             (longword?(in.src.is_mem()?6:8):4)+ea_read(in.src,in.size));break;
    case Op::Eor:cost(memory?(longword?12:8)+ea_read(in.dst,in.size):(longword?8:4));break;
    case Op::Andi:case Op::Ori:case Op::Eori:case Op::Addi:case Op::Subi:
        cost(memory?(longword?20:12)+ea_read(in.dst,in.size):(longword?16:8));break;
    case Op::Cmpi:cost(memory?(longword?12:8)+ea_read(in.dst,in.size):(longword?14:8));break;
    case Op::Cmp:cost((longword?6:4)+ea_read(in.src,in.size));break;
    case Op::Cmpa:cost(6+ea_read(in.src,in.size));break;
    case Op::Adda:case Op::Suba:cost((longword?(in.src.is_mem()?6:8):8)+ea_read(in.src,in.size));break;
    case Op::Addq:case Op::Subq:
        cost(in.dst.mode==Mode::An?8:memory?(longword?12:8)+ea_read(in.dst,in.size):(longword?8:4));break;
    case Op::Btst:case Op::Bchg:case Op::Bclr:case Op::Bset: {
        const bool immediate=in.src.mode==Mode::Imm;
        if(in.dst.mode==Mode::Dn) {
            cost((in.op==Op::Bclr?8:6)+(immediate?4:0));
            if(in.op!=Op::Btst)result.dynamic=TimingDynamic::BitRegister;
        } else cost((in.op==Op::Btst?(in.dst.mode==Mode::Imm?6:4):8)+(immediate?4:0)+ea_read(in.dst,1));
        break;
    }
    case Op::Asl:case Op::Asr:case Op::Lsl:case Op::Lsr:case Op::Roxl:case Op::Roxr:case Op::Rol:case Op::Ror:
        if(in.src.mode==Mode::None)cost(8+ea_read(in.dst,2));
        else {cost(longword?8:6);result.dynamic=TimingDynamic::Shift;}break;
    case Op::Mulu:case Op::Muls:
        cost(38+ea_read(in.src,2));result.dynamic=in.op==Op::Mulu?TimingDynamic::MultiplyUnsigned:TimingDynamic::MultiplySigned;break;
    case Op::Scc:
        cost(memory?8+ea_read(in.dst,1):4);result.dummy_destination_read=memory;
        if(!memory)result.dynamic=TimingDynamic::Scc;break;
    case Op::Bra:cost(10);result.fetch=TimingFetch::Branch;break;
    case Op::Bsr:cost(18);result.fetch=TimingFetch::Branch;break;
    case Op::Bcc:cost(10);result.dynamic=TimingDynamic::Branch;result.fetch=TimingFetch::Branch;break;
    case Op::Dbcc:cost(10);result.dynamic=TimingDynamic::Dbcc;result.fetch=TimingFetch::Dbcc;break;
    case Op::Jmp:case Op::Jsr: {
        unsigned extra=control_ea(in.src);
        if(in.src.mode==Mode::AbsL)extra-=4;
        if(in.src.mode==Mode::Disp||in.src.mode==Mode::AbsW||in.src.mode==Mode::PcDisp||in.src.mode==Mode::Index||in.src.mode==Mode::PcIndex)extra-=2;
        cost((in.op==Op::Jmp?8:16)+extra);result.fetch=TimingFetch::Jump;break;
    }
    case Op::Rts:cost(16);result.fetch=TimingFetch::Return;break;
    case Op::Rte:case Op::Rtr:cost(20);result.fetch=TimingFetch::Return;break;
    case Op::MoveToSr:case Op::MoveToCcr:cost(12+ea_read(in.src,2));result.fetch=TimingFetch::Status;break;
    case Op::MoveFromSr:
        cost(memory?8+ea_read(in.dst,2):6);result.dummy_destination_read=memory;break;
    case Op::OriSr:case Op::AndiSr:case Op::EoriSr:case Op::OriCcr:case Op::AndiCcr:case Op::EoriCcr:
        cost(20);result.fetch=TimingFetch::Status;break;
    case Op::Movem: {
        const auto &ea=in.to_mem?in.dst:in.src;
        unsigned extra=control_ea(ea);
        if(ea.mode==Mode::Index||ea.mode==Mode::PcIndex)extra-=2;
        cost((in.to_mem?8:12)+extra+std::popcount(in.mask)*(longword?8:4));
        result.movem_trailing_read=!in.to_mem;break;
    }
    case Op::Link:cost(16);break;
    case Op::Unlk:cost(12);break;
    case Op::MoveUsp:cost(4);break;
    case Op::Exg:cost(6);break;
    default:return std::nullopt;
    }
    return result;
}
} // namespace m68k
