// Shared-clock scheduling fixture. The generated-entry stub below is synthetic;
// instruction semantics/totals are covered by the separate 68000 oracle tests.
#include "runtime/model2a_sound.h"
#include "runtime/snd_gen_support.h"
#include <cstdio>
#include <stdexcept>

namespace {
enum class Program { Frontier, Midi, MidiAfterHold, Hold, BadCount } program;
unsigned checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
snd::Model2ASound::Images images() {
    snd::Model2ASound::Images result;
    result.program.resize(0x80000);
    result.samples.resize(0x800000);
    const auto put = [&](unsigned address, uint32_t value) {
        for (int byte = 3; byte >= 0; --byte) {
            result.program[address + byte] = uint8_t(value);
            value >>= 8;
        }
    };
    put(0, 0x5000);
    put(4, 0x600100);
    return result;
}
}

namespace sndgen {
bool has_code(uint32_t pc) { return pc == 0x600100 || pc == 0x600102 || pc == 0x600180; }
uint64_t native_instructions() { return 3; }
void run(Env &env) {
    auto &c = env.c;
    auto &s = env.s;
    if (s.boundary()) return;
    if (program == Program::BadCount) return;
    c.begin_instruction();
    unsigned cycles = 4;
    if (program != Program::Frontier && c.pc == 0x600100) {
        c.w32(25 * 4, 0x600180);
        const unsigned enabled = program == Program::Midi ? 8 :
            program == Program::Hold ? 0x10 : 0x18;
        c.w16(0x100424, enabled); // Actual SCSP MIDI and/or DMA IRQ -> IPL1.
        c.w16(0x10041e, enabled);
        if (program == Program::Hold || program == Program::MidiAfterHold) {
            c.w16(0x100412, 0x100);
            c.w16(0x100414, 0x704);
            c.w16(0x100416, 0x1002); // Immediate DMA generates HOLD_LINE.
        }
        if (program != Program::MidiAfterHold) c.set_sr(0x2000);
        c.pc = 0x600102;
        cycles = 48;
    } else if (program != Program::Frontier && c.pc == 0x600180) {
        if (program != Program::Hold)
            c.d[0] = c.r16(0x100404) & 0xff; // Reading received byte clears IRQ.
        c.pc = 0x600102;
        cycles = 8;
    } else if (program == Program::MidiAfterHold && ++c.d[1] == 1000) {
        c.set_sr(0x2000); // Unmask after physical MIDI ASSERT replaced HOLD.
    } else if (program == Program::Frontier) {
        c.pc += 2;
    }
    c.account_prefetch(c.pc + 2);
    c.finish_instruction(cycles);
    ++s.count;
}
}

namespace {
void boundaries_and_frontier() {
    program = Program::Frontier;
    constexpr uint64_t origin = 123456;
    rt::Model2AClock clock(origin);
    snd::Model2Scsp::ReferenceRandom random;
    const auto first = origin + snd::Cpu68k::model2a_time_ns(38);
    bool before = false, after = false;
    snd::Model2ASound *board = nullptr;
    clock.schedule_at(first, [&](uint64_t) {
        before = true;
        require(board->stats().instructions == 0, "earlier tied event ran after CPU");
    });
    {
        snd::Model2ASound sound(clock, images(), [&] { return random(); }, 8);
        board = &sound;
        require(clock.now_ns() == origin, "constructor advanced shared clock");
        require(sound.stats().cycles == 38, "reset cycles missing");
        clock.schedule_at(first, [&](uint64_t) {
            after = true;
            require(sound.stats().instructions == 1, "later tied event ran before CPU");
        });
        clock.advance(first - 1);
        require(sound.stats().instructions == 0, "instruction ran before reset deadline");
        clock.advance(first);
        require(before && after, "tied event order not observed");
        require(sound.stats().instructions == 1 && sound.cpu().pc == 0x600102,
                "callback executed more than one instruction");
        const auto second = origin + snd::Cpu68k::model2a_time_ns(42);
        clock.advance(second);
        require(sound.stats().instructions == 2 && sound.cpu().pc == 0x600104,
                "second instruction boundary incorrect");
        const auto missing = origin + snd::Cpu68k::model2a_time_ns(46);
        bool failed = false;
        try { clock.advance(missing + 100); } catch (const rt::Fatal &) { failed = true; }
        require(failed && clock.now_ns() == missing, "missing code did not stop at its deadline");
        const auto state = sound.stats();
        require(state.status == snd::Model2ASound::Status::MissingCode &&
                state.error.find("600104") != std::string::npos, "missing frontier lost PC");
        clock.advance(missing + 100000);
        require(sound.stats().instructions == state.instructions && sound.stats().cycles == state.cycles,
                "stopped CPU rescheduled itself");
        require(sound.stats().sample_frames > 0, "SCSP stopped with CPU frontier");
    }
    require(clock.snapshot().pending.empty(), "destroyed board left clock callbacks");
    clock.advance(clock.now_ns() + 1000000);
}

void physical_midi_interrupt(bool initial_hold) {
    program = initial_hold ? Program::MidiAfterHold : Program::Midi;
    rt::Model2AClock clock;
    snd::Model2Scsp::ReferenceRandom random;
    {
        snd::Model2ASound sound(clock, images(), [&] { return random(); }, 32);
        // Send 8N1 at 31.25 kbit/s over the real SCSP input pin.
        constexpr uint64_t start = 10000, bit_ns = 32000;
        constexpr uint8_t byte = 0xa5;
        sound.midi_in(true);
        clock.schedule_at(start, [&](uint64_t) { sound.midi_in(false); });
        for (unsigned bit = 0; bit < 8; ++bit)
            clock.schedule_at(start + (bit + 1) * bit_ns, [&, bit](uint64_t) {
                sound.midi_in((byte >> bit) & 1);
            });
        clock.schedule_at(start + 9 * bit_ns, [&](uint64_t) { sound.midi_in(true); });
        clock.advance(start + 250000);
        require(sound.stats().irq_entries == 0, "MIDI interrupt arrived before byte completed");
        bool saw_irq = false;
        while (clock.next_deadline() && *clock.next_deadline() < start + 400000) {
            const auto count = sound.stats().instructions;
            clock.advance(*clock.next_deadline());
            if (sound.stats().irq_entries) {
                require(sound.stats().instructions == count, "IRQ callback also executed an instruction");
                require(sound.cpu().pc == 0x600180 && sound.cpu().mask() == 1,
                        "real SCSP interrupt did not enter installed vector");
                require(sound.cpu().irq_line == 1, "acknowledgment cleared ASSERT_LINE");
                saw_irq = true;
                break;
            }
        }
        require(saw_irq, "physical MIDI byte did not interrupt sound CPU");
        clock.advance(start + 400000);
        require(sound.cpu().d[0] == byte, "sound handler did not read physical MIDI byte");
        require(sound.cpu().irq_line == 0 && sound.stats().irq_entries == 1,
                "SCSP IRQ did not clear after data read");
        require(sound.take_samples().size() == sound.stats().sample_frames,
                "sample drain lost frames below ring capacity");
    }
    require(clock.snapshot().pending.empty(), "live CPU/chip destruction left callbacks");
}

void dma_hold_acknowledgment() {
    program = Program::Hold;
    rt::Model2AClock clock;
    snd::Model2Scsp::ReferenceRandom random;
    snd::Model2ASound sound(clock, images(), [&] { return random(); }, 0);
    clock.advance(snd::Cpu68k::model2a_time_ns(38));
    require(sound.cpu().irq_line == 1 && sound.stats().instructions == 1,
            "DMA did not hold IPL1");
    clock.advance(snd::Cpu68k::model2a_time_ns(sound.stats().cycles));
    require(sound.cpu().irq_line == 0 && sound.stats().irq_entries == 1 &&
            sound.stats().instructions == 1, "IACK did not clear HOLD_LINE");
}

void bad_generated_boundary() {
    program = Program::BadCount;
    rt::Model2AClock clock;
    snd::Model2Scsp::ReferenceRandom random;
    snd::Model2ASound sound(clock, images(), [&] { return random(); }, 0);
    bool failed = false;
    try { clock.advance(10000); } catch (const rt::Fatal &) { failed = true; }
    require(failed && sound.stats().status == snd::Model2ASound::Status::RuntimeError,
            "nonprogressing generated callback was rescheduled");
    clock.advance(20000);
    require(sound.stats().instructions == 0, "failed generated callback ran again");
}
}

int main() {
    boundaries_and_frontier();
    physical_midi_interrupt(false);
    physical_midi_interrupt(true);
    dma_hold_acknowledgment();
    bad_generated_boundary();
    std::printf("Model2A sound board scheduling: %u checks passed\n", checks);
}
