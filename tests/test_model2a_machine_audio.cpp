// Synthetic generated programs exercise the real shared machine and SCSP.
#include "runtime/model2a_machine.h"
#include "runtime/gen_support.h"
#include "runtime/snd_gen_support.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

namespace gen {
bool has_code(uint32_t pc) { return pc == 0x420; }
uint64_t native_instructions() { return 1; }
void run(Env &e) {
    while (!e.ls.boundary()) {
        e.c.consume_reference_cycles(25000); // Synthetic 1ms instruction.
        ++e.ls.count;
    }
}
}
namespace sndgen {
bool has_code(uint32_t pc) { return pc == 0x600100 || pc == 0x600102; }
uint64_t native_instructions() { return 2; }
void run(Env &e) {
    if (e.s.boundary()) return;
    auto &c = e.c;
    c.begin_instruction();
    if (c.pc == 0x600100) {
        for (unsigned i=0;i<8;++i) c.w16(0x100+i*2,0x1000);
        c.w16(0x100400,15); c.w16(0x100002,0x100);
        c.w16(0x100004,0); c.w16(0x100006,4);
        c.w16(0x10000c,0x100); // Direct samples; no envelope/TL.
        c.w16(0x100016,0xff00); // Left output, maximum send, right muted.
        c.w16(0x100000,0x1820); // Key on, normal loop.
        c.pc=0x600102;
    }
    c.account_prefetch(c.pc+2);
    c.finish_instruction(40);
    ++e.s.count;
}
}
namespace {
using Machine=rt::Model2AMachine;
Machine::Images images() {
    Machine::Images result;
    result.program.resize(32);result.program[12]=0x20;result.program[13]=4;
    result.sound_program.resize(0x80000); result.sound_samples.resize(0x800000);
    result.sound_program[2]=0x50;
    result.sound_program[5]=0x60;result.sound_program[6]=1;
    return result;
}
Machine::Options options(bool sound) {
    Machine::Options o;
    o.tgp=o.io=o.comm=o.video=o.screen=o.external_video=false;
    o.control=o.uart=true;o.sound=sound;
    return o;
}
void unchanged(const Machine::Stats &a,const Machine::Stats &b) {
    assert(a.instructions==b.instructions && a.reference_cycles==b.reference_cycles && a.pc==b.pc);
    assert(a.board_time_ns==b.board_time_ns && a.irq_entries==b.irq_entries);
    assert(a.sound_instructions==b.sound_instructions && a.sound_cycles==b.sound_cycles);
    assert(a.sound_pc==b.sound_pc && a.sound_irqs==b.sound_irqs);
    assert(a.sound_samples==b.sound_samples && a.sound_dropped_samples==b.sound_dropped_samples);
}
}
int main() {
    Machine disabled(images(),options(false));
    const auto off=disabled.stats();assert(disabled.take_audio().empty());unchanged(off,disabled.stats());
    Machine frequent(images(),options(true)), batched(images(),options(true));
    std::vector<float> continuous;
    for(unsigned i=0;i<100;++i) {
        assert(frequent.run_for(1).reason==Machine::Reason::Budget);
        const auto before=frequent.stats();const auto block=frequent.take_audio();
        unchanged(before,frequent.stats());assert(frequent.take_audio().empty());
        continuous.insert(continuous.end(),block.begin(),block.end());
    }
    assert(batched.run_for(100).reason==Machine::Reason::Budget);
    unchanged(frequent.stats(),batched.stats());
    assert(batched.take_audio()==continuous);
    assert(continuous.size()==frequent.stats().sound_samples*2 && continuous.size()>8000);
    assert(std::count(continuous.begin(),continuous.end(),0.125f)>4000);
    for(size_t i=0;i<continuous.size();i+=2) {
        assert(continuous[i]==0 || continuous[i]==0.125f);
        assert(continuous[i+1]==0); // Preserves interleaved left/right order.
    }
    Machine stalled(images(),options(true));
    assert(stalled.run_for(1100).reason==Machine::Reason::Budget);
    const auto state=stalled.stats();const auto tail=stalled.take_audio();
    unchanged(state,stalled.stats());
    assert(tail.size()==2*Machine::kAudioRate);
    assert(state.sound_dropped_samples==state.sound_samples-Machine::kAudioRate);
    assert(stalled.take_audio().empty());
    std::puts("Machine audio: exact PCM, drain invariance, disabled path and bounded overflow passed");
}
