#include "runtime/snd_cpu.h"

#include <cstdio>
#include <limits>

namespace snd {

void Cpu68k::reset() {
    if (timing_active_ || cycle_callback_active_) fatal("reentrant sound reset");
    last_vpa_cycle_ = 0;
    sys = 0x2700;
    set_ccr(0);
    if (model2a_timing) begin_instruction();
    a[7] = r32(0);
    pc = r32(4);
    if (model2a_timing) {
        account_prefetch(pc); account_prefetch(pc+2);
        // Pinned reset microcode: six bus completions + five internal steps.
        finish_instruction(34);
    }
}

void Cpu68k::set_sr(uint32_t v16) {
    const bool was_super = sys & 0x2000;
    sys = uint16_t(v16 & 0xa700);
    set_ccr(v16);
    if (was_super != bool(sys & 0x2000)) std::swap(a[7], other_sp);
}

void Cpu68k::take_irq(int level) {
    if (timing_active_ || cycle_callback_active_) fatal("reentrant sound interrupt entry");
    if (model2a_timing && (level<1 || level>7)) fatal("invalid timed interrupt level");
    const uint16_t old = sr();
    if (!(sys & 0x2000)) std::swap(a[7], other_sp); // enter supervisor
    sys = uint16_t((sys & ~0x8700) | 0x2000 | (level << 8));
    if (model2a_timing) {
        begin_instruction();
        // Source writes PC low first, acknowledges in CPU space, then writes
        // SR and PC high. CPU-space IACK never touches the samples ROM.
        w16(a[7]-2,pc);
        if(on_irq_ack)on_irq_ack(level);
        if(level==7)nmi_pending=false;
        const uint64_t before_iack=10+pending_cycles_.mapped_waits;
        if(before_iack>std::numeric_limits<uint64_t>::max()-cycles)fatal("sound IRQ cycle overflow");
        const uint64_t now=cycles+before_iack;
        if(last_vpa_cycle_<now) {
            const uint64_t delay=(now%10<7?10:20)-now%10;
            if(delay>std::numeric_limits<uint64_t>::max()-now)fatal("sound IACK cycle overflow");
            last_vpa_cycle_=now+delay;
        }
        pending_cycles_.iack_transfers=1;
        pending_cycles_.iack_waits=last_vpa_cycle_-now+1;
        w16(a[7]-6,old);w16(a[7]-4,pc>>16);a[7]-=6;
    } else {
        push32(pc);
        push16(old);
    }
    pc = r32(uint32_t(24 + level) * 4);
    if (model2a_timing) {account_prefetch(pc);account_prefetch(pc+2);finish_instruction(44);}
}

uint32_t Cpu68k::model2a_wait(uint32_t address) {
    address&=0xffffff;
    return address<0x80000 || (address>=0x100000 && address<0x101000) ? 1 : 0;
}
uint64_t Cpu68k::model2a_time_ns(uint64_t count) {
    constexpr uint64_t rate=11'289'600, billion=1'000'000'000;
    const auto whole=count/rate, part=count%rate;
    if(whole>std::numeric_limits<uint64_t>::max()/billion)
        throw rt::Fatal("sound nanosecond clock overflow");
    const uint64_t base=whole*billion, fraction=(part*billion+rate-1)/rate;
    if(fraction>std::numeric_limits<uint64_t>::max()-base)
        throw rt::Fatal("sound nanosecond clock overflow");
    return base+fraction;
}
// Cycle-only adaptation of Olivier Galibert's BSD-3-Clause M68000 microcode,
// MAME dddd73680656e355bb2b5beecab1167c9f07bf81. The fixed16-step loops
// count actual subtraction/restore paths; no instruction is interpreted.
uint32_t Cpu68k::divu_cycles(uint32_t dividend, uint16_t divisor) {
    if (!divisor) throw rt::Fatal("timed sound divide-by-zero exception is unsupported");
    if ((dividend >> 16) >= divisor) return 10;
    uint32_t cycles = 76, remainder = dividend >> 16, low = dividend & 0xffff;
    for (unsigned i = 0; i < 16; ++i) {
        const bool high = remainder & 0x8000;
        remainder = (remainder << 1) | (low >> 15);
        low = (low << 1) & 0xffff;
        const bool borrow = remainder < divisor;
        if (!borrow) { remainder -= divisor; low |= 1; }
        // Last-step exit is included in76. Earlier nonnegative remainders
        // test carry; unsuccessful subtraction additionally restores it.
        if (i < 15) { if (!high) cycles += 2; if (borrow) cycles += 2; }
    }
    return cycles;
}
uint32_t Cpu68k::divs_cycles(uint32_t dividend, uint16_t divisor) {
    if (!divisor) throw rt::Fatal("timed sound divide-by-zero exception is unsupported");
    const bool negative_dividend = dividend & 0x80000000, negative_divisor = divisor & 0x8000;
    const uint32_t magnitude = negative_dividend ? 0u - dividend : dividend;
    const uint32_t denominator = negative_divisor ? uint16_t(0u - divisor) : divisor;
    if ((magnitude >> 16) >= denominator) return 16 + (negative_dividend ? 2 : 0);
    uint32_t cycles = 120 + (negative_dividend ? 2 : 0) + (negative_divisor ? 2 : negative_dividend ? 4 : 0);
    uint32_t remainder = magnitude >> 16, low = magnitude & 0xffff;
    for (unsigned i = 0; i < 16; ++i) {
        remainder = (remainder << 1) | (low >> 15);
        low = (low << 1) & 0xffff;
        if (remainder >= denominator) { remainder -= denominator; low |= 1; }
        else if (i < 15) cycles += 2;
    }
    return cycles;
}
void Cpu68k::begin_instruction() {
    if(!model2a_timing)fatal("timed sound code requires explicit Model2A accounting");
    if(timing_active_ || cycle_callback_active_)fatal("reentrant sound cycle accounting");
    if(sys&0x8000)fatal("timed sound trace exception is unsupported");
    pending_cycles_={};timing_active_=true;
}
void Cpu68k::account_data(uint32_t address) {
    ++pending_cycles_.data_transfers;
    pending_cycles_.mapped_waits+=model2a_wait(address);
}
void Cpu68k::account_prefetch(uint32_t address) {
    if(!timing_active_)fatal("sound prefetch outside timed instruction");
    address&=0xffffff;
    if(address&1)odd(address);
    if(!(address<0x80000 || (address>=0x600000 && address<0x680000)))
        fatal("timed prefetch outside sound RAM/program is unsupported");
    ++pending_cycles_.prefetch_transfers;
    pending_cycles_.mapped_waits+=model2a_wait(address);
}
void Cpu68k::dummy_read(uint32_t address,unsigned size) {
    if(!timing_active_)fatal("sound dummy read outside timed instruction");
    // Actual reference dummy reads retain MMIO side effects. These accesses
    // are additional to the inherited semantic templates, only in timed mode.
    if(size==1) {++pending_cycles_.dummy_reads;(void)r8(address);}
    else if(size==2) {++pending_cycles_.dummy_reads;(void)r16(address);}
    else if(size==4) {pending_cycles_.dummy_reads+=2;(void)r32(address);}
    else fatal("invalid sound dummy read size");
}
void Cpu68k::finish_instruction(uint32_t nominal) {
    if(!timing_active_ || cycle_callback_active_)fatal("sound cycle finish outside instruction");
    const uint64_t elapsed=uint64_t(nominal)+pending_cycles_.mapped_waits+pending_cycles_.iack_waits;
    if(elapsed>std::numeric_limits<uint64_t>::max()-cycles)fatal("sound cycle counter overflow");
    auto add=[&](uint64_t &total,uint64_t amount){
        if(amount>std::numeric_limits<uint64_t>::max()-total)fatal("sound bus counter overflow");total+=amount;
    };
    auto next_stats = cycle_stats;
    add(next_stats.data_transfers,pending_cycles_.data_transfers);
    add(next_stats.prefetch_transfers,pending_cycles_.prefetch_transfers);
    add(next_stats.dummy_reads,pending_cycles_.dummy_reads);
    add(next_stats.mapped_waits,pending_cycles_.mapped_waits);
    add(next_stats.iack_transfers,pending_cycles_.iack_transfers);
    add(next_stats.iack_waits,pending_cycles_.iack_waits);
    cycle_stats = next_stats;
    cycles+=elapsed;timing_active_=false;
    if(on_cycles) {
        cycle_callback_active_=true;
        try {on_cycles(cycles);cycle_callback_active_=false;}
        catch(...) {cycle_callback_active_=false;throw;}
    }
}

namespace {
bool dev_range(uint32_t a) {
    return (a >= 0xc20000 && a <= 0xc20003) || (a >= 0xc40000 && a <= 0xc40007) || (a >= 0xc60000 && a <= 0xc60007) ||
           (a >= 0xd00000 && a <= 0xd00007);
}
} // namespace

// 8-bit devices answer on the odd byte lane only; the even lane and unmapped
// addresses read 0 (MAME's unmap value for this space).
uint8_t Cpu68k::dev_r8(uint32_t addr) {
    if ((addr & 1) && dev_range(addr)) return dev->read(addr);
    return 0;
}

uint16_t Cpu68k::dev_r16(uint32_t addr) {
    if (dev_range(addr)) return dev->read(addr | 1);
    return 0;
}

void Cpu68k::dev_w(uint32_t addr, uint16_t data, uint16_t mem_mask) {
    const uint32_t even = addr & ~1u;
    if (dev_range(addr)) {
        if (mem_mask & 0x00ff) dev->write(even | 1, data & 0xff);
    } else if (even == 0xc40012 || even == 0xc50000 || even == 0xc70000) {
        dev->write(even, data & mem_mask);
    }
}

void Cpu68k::fatal(const std::string &what) const {
    char b[64];
    std::snprintf(b, sizeof b, " (sound 68000, pc %06x)", pc);
    throw rt::Fatal(what + b);
}

void Cpu68k::odd(uint32_t addr) const {
    char b[64];
    std::snprintf(b, sizeof b, "word access at odd address %06x", addr);
    fatal(b);
}

} // namespace snd
