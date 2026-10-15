// Test-only independently executing pinned-MAME 68000, on separate shared-model
// devices. This is instruction-start MMIO, not physical bus-phase equivalence.
// The reference owns registers/flags/prefetch/reset/IRQ/cycle deductions. No
// snd::Cpu68k instruction, IRQ or reset helper executes on the reference side.
#pragma once
#include "m68000-board-reference.h"
#include "sound-reference-program.h"
#include "runtime/model2a_sound.h"
#include <algorithm>
#include <limits>

namespace sound_reference {
inline uint64_t time_ns(uint64_t cycles) {
    constexpr uint64_t rate=11'289'600,billion=1'000'000'000;
    const auto whole=cycles/rate,part=cycles%rate;
    if(whole>std::numeric_limits<uint64_t>::max()/billion) throw rt::Fatal("reference clock overflow");
    const auto base=whole*billion,fraction=(part*billion+rate-1)/rate;
    if(fraction>std::numeric_limits<uint64_t>::max()-base) throw rt::Fatal("reference clock overflow");
    return base+fraction;
}
class OracleBoard {
public:
    struct Stats {uint64_t instructions=0,cycles=0,irq_entries=0;uint32_t pc=0,previous_dispatch=0;};
    struct Access {uint64_t board_ns,phase_cycle;char kind;uint32_t address;uint16_t data,mask;bool write;};
    OracleBoard(rt::Model2AClock &clock,snd::Model2ASound::Images images,
                snd::Model2Scsp::RandomProvider random,size_t capacity)
        :clock_(clock),images_(std::move(images)),bus_(images_.program,images_.samples),
         chip_(clock,bus_.sound_ram(),std::move(random),capacity),origin_(clock.now_ns()) {
        bus_.set_scsp(&chip_);cpu_.waits=true;
        cpu_.mapped_access=[this](char kind,uint32_t address,uint16_t data,uint16_t mask,bool write) {
            if(accesses_.size()==65536) throw rt::Fatal("reference access evidence bound");
            if(clock_.now_ns()!=callback_ns_) throw rt::Fatal("reference MMIO advanced board clock inside callback");
            uint16_t value=data&mask;
            if(kind=='i') {
                const unsigned level=cpu_.m_next_state>>24;
                value=uint16_t(24+level)&mask; // Actual autovector returned by the pinned CPU-space port.
                if(held_&(1u<<level)) {held_&=~(1u<<level);lines_&=~(1u<<level);update_irq();}
            } else if(write) {
                // This first profile admits immutable ROM execution only. Do
                // not pretend a stale prefetch/self-modifying stream matches
                // the native guarded instructions by resetting setup() each op.
                if(address>=0x600000 && address<0x680000) throw rt::Fatal("reference self-modifying ROM profile unsupported");
                bus_.write16(address&~1u,data,mask);
            } else value=bus_.read16(address&~1u,mask);
            accesses_.push_back({clock_.now_ns(),cpu_.cycle_origin+uint64_t(100000000-cpu_.m_icount),kind,address,value,mask,write});
            return value;
        };
        chip_.on_irq=[this](unsigned level,snd::Model2Scsp::LineState state) {
            if(level>7) throw rt::Fatal("reference invalid IPL");
            if(state==snd::Model2Scsp::LineState::Clear) {lines_&=~(1u<<level);held_&=~(1u<<level);}
            else {lines_|=1u<<level;if(state==snd::Model2Scsp::LineState::Hold) held_|=1u<<level;else held_&=~(1u<<level);}
            update_irq();
        };
        chip_.on_midi_tx=[this](bool value) {if(on_midi_tx) on_midi_tx(value);};
        callback_ns_=clock_.now_ns();begin();cpu_.state_reset();finish();schedule();
    }
    ~OracleBoard() {clock_.cancel(next_);}
    OracleBoard(const OracleBoard &)=delete;
    OracleBoard &operator=(const OracleBoard &)=delete;
    std::function<void(bool)> on_midi_tx;
    void midi_in(bool value) {chip_.midi_in(value);}
    const MicroOracle &cpu()const{return cpu_;}
    const snd::Model2ASoundBus &bus()const{return bus_;}
    int irq_line()const{return irq_;}
    bool nmi_pending()const{return nmi_;}
    const std::vector<Access> &accesses()const{return accesses_;}
    Stats stats()const{auto result=stats_;result.pc=cpu_.m_pc-2;return result;}
    std::vector<snd::Model2Scsp::Sample> take_samples(){return chip_.take_samples();}
    uint64_t sample_count()const{return chip_.sample_count();}
    uint64_t dropped_samples()const{return chip_.dropped_samples();}
private:
    rt::Model2AClock &clock_;
    snd::Model2ASound::Images images_;
    snd::Model2ASoundBus bus_;
    snd::Model2Scsp chip_;
    MicroOracle cpu_;
    rt::Model2AClock::Token next_{};
    uint64_t origin_,callback_ns_=0;
    Stats stats_;
    unsigned lines_=0,held_=0;
    int irq_=0;
    bool nmi_=false;
    std::vector<Access> accesses_;
    void update_irq() {
        const int old=irq_;irq_=0;
        for(int level=7;level>0;--level) if(lines_&(1u<<level)) {irq_=level;break;}
        if(old!=7 && irq_==7) nmi_=true;
    }
    void begin() {
        cpu_.m_icount=100000000;cpu_.cycle_origin=stats_.cycles;cpu_.accesses.clear();
        // last_vpa_time and all prefetch/ALU/SR/stack state persist.
    }
    void finish() {
        if(cpu_.m_inst_substate || cpu_.m_inst_state || cpu_.m_next_state)
            throw rt::Fatal("reference exception/partial instruction profile unsupported");
        const int delta=100000000-cpu_.m_icount;
        if(delta<=0 || delta>1000000) throw rt::Fatal("reference invalid cycle progress");
        if(uint64_t(delta)>std::numeric_limits<uint64_t>::max()-stats_.cycles) throw rt::Fatal("reference cycles overflow");
        stats_.cycles+=unsigned(delta);
    }
    void schedule() {
        const auto offset=time_ns(stats_.cycles);
        if(offset>std::numeric_limits<uint64_t>::max()-origin_) throw rt::Fatal("reference deadline overflow");
        const auto deadline=origin_+offset;
        if(deadline<=clock_.now_ns()) throw rt::Fatal("reference nonprogressing deadline");
        next_=clock_.schedule_at(deadline,[this](uint64_t now){step(now);});
    }
    void step(uint64_t now) {
        next_={};callback_ns_=now;begin();
        if(nmi_ || irq_>int((cpu_.m_sr>>8)&7)) {
            const unsigned level=nmi_?7:unsigned(irq_);
            cpu_.m_next_state=(level<<24)|MicroOracle::S_INTERRUPT;
            cpu_.state_interrupt();if(level==7)nmi_=false;++stats_.irq_entries;
        } else {
            const uint32_t pc=cpu_.m_pc-2;
            const auto found=std::find_if(program_profile.begin(),program_profile.end(),[&](auto entry){return entry.first==pc;});
            if(found==program_profile.end() || pc<0x600000 || pc+1>=0x680000)
                throw rt::Fatal("reference instruction not in authenticated synthetic ROM profile");
            const auto offset=pc-0x600000;
            const uint16_t word=uint16_t(images_.program[offset]<<8|images_.program[offset+1]);
            if(cpu_.m_ird!=found->second || word!=found->second)
                throw rt::Fatal("reference persistent prefetch differs from live guarded opcode");
            // Pinned execute_run instruction preparation, not setup(): keep the
            // prefetched extension/opcode words and every other CPU field.
            cpu_.m_ipc=pc;cpu_.m_irdi=cpu_.m_ird;
            cpu_.run(cpu_.m_irdi);stats_.previous_dispatch=pc;++stats_.instructions;
        }
        finish();schedule();
    }
};
} // namespace sound_reference
