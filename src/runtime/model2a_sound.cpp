#include "runtime/model2a_sound.h"
#include "runtime/snd_gen_support.h"
#include <cstdio>
#include <limits>

namespace snd {
struct Model2ASound::Impl {
    Model2ASound &owner;
    rt::Model2AClock &clock;
    Images images; // Bus image spans remain valid for the whole board lifetime.
    Model2ASoundBus bus;
    Model2Scsp chip;
    Cpu68k cpu;
    Sched schedule;
    sndgen::Env env;
    rt::Model2AClock::Token next{};
    uint64_t origin, irq_entries = 0;
    unsigned irq_lines = 0, held_lines = 0;
    uint32_t previous_dispatch = 0;
    Status status = Status::Running;
    std::string error;

    Impl(Model2ASound &owner_, rt::Model2AClock &clock_, Images images_,
         Model2Scsp::RandomProvider random, size_t capacity)
        : owner(owner_), clock(clock_), images(std::move(images_)),
          bus(images.program, images.samples), chip(clock, bus.sound_ram(), std::move(random), capacity),
          schedule(cpu), env{cpu, schedule}, origin(clock.now_ns()) {
        cpu.bus = &bus;
        bus.set_scsp(&chip);
        cpu.model2a_timing = true;
        schedule.auto_irq = false;
        cpu.on_irq_ack = [this](int level) {
            if (held_lines & (1u << level)) {
                held_lines &= ~(1u << level);
                irq_lines &= ~(1u << level);
                update_irq();
            }
        };
        chip.on_irq = [this](unsigned level, Model2Scsp::LineState state) {
            if (level > 7) throw rt::Fatal("SCSP interrupt level exceeds 68000 IPL");
            if (state == Model2Scsp::LineState::Clear) {
                irq_lines &= ~(1u << level);
                held_lines &= ~(1u << level);
            } else {
                irq_lines |= 1u << level;
                if (state == Model2Scsp::LineState::Hold) held_lines |= 1u << level;
                else held_lines &= ~(1u << level);
            }
            update_irq();
        };
        chip.on_midi_tx = [this](bool level) { if (owner.on_midi_tx) owner.on_midi_tx(level); };
        // No on_cycles callback advances time. Reset accounts its bus cycles,
        // then schedules the first instruction without moving the shared clock.
        cpu.reset();
        schedule_next();
    }
    ~Impl() { clock.cancel(next); }
    void update_irq() {
        const int previous = cpu.irq_line;
        cpu.irq_line = 0;
        for (int level = 7; level > 0; --level) {
            if (irq_lines & (1u << level)) { cpu.irq_line = level; break; }
        }
        if (previous != 7 && cpu.irq_line == 7) cpu.nmi_pending = true;
    }
    void schedule_next() {
        const uint64_t offset = Cpu68k::model2a_time_ns(cpu.cycles);
        if (offset > std::numeric_limits<uint64_t>::max() - origin)
            throw rt::Fatal("sound board deadline overflow");
        const uint64_t deadline = origin + offset;
        if (deadline <= clock.now_ns()) throw rt::Fatal("sound CPU did not advance its cycle deadline");
        next = clock.schedule_at(deadline, [this](uint64_t) { step(); });
    }
    void step() {
        next = {};
        if (status != Status::Running) return;
        try {
            if (cpu.nmi_pending || cpu.irq_line > cpu.mask()) {
                cpu.take_irq(cpu.nmi_pending ? 7 : cpu.irq_line);
                ++irq_entries;
            } else {
                if (!sndgen::has_code(cpu.pc)) {
                    status = Status::MissingCode;
                    throw rt::Fatal("native sound code target is not compiled");
                }
                if (schedule.count == UINT64_MAX) throw rt::Fatal("sound instruction count overflow");
                const uint64_t before = schedule.count;
                schedule.end_count = before + 1;
                previous_dispatch = cpu.pc;
                sndgen::run(env);
                if (schedule.count != before + 1)
                    throw rt::Fatal("sound callback did not execute exactly one instruction");
            }
            schedule_next();
        } catch (const std::exception &failure) {
            if (status == Status::Running) status = Status::RuntimeError;
            char address[16];
            std::snprintf(address, sizeof address, "%06x", cpu.pc);
            error = std::string("sound CPU at 0x") + address + ": " + failure.what();
            throw rt::Fatal(error);
        }
    }
};

Model2ASound::Model2ASound(rt::Model2AClock &clock, Images images,
    Model2Scsp::RandomProvider random, size_t capacity)
    : impl_(std::make_unique<Impl>(*this, clock, std::move(images), std::move(random), capacity)) {}
Model2ASound::~Model2ASound() = default;
void Model2ASound::midi_in(bool level) { impl_->chip.midi_in(level); }
std::vector<Model2Scsp::Sample> Model2ASound::take_samples() { return impl_->chip.take_samples(); }
const Cpu68k &Model2ASound::cpu() const { return impl_->cpu; }
Model2ASound::Stats Model2ASound::stats() const {
    const auto &m = *impl_;
    return {m.status, m.schedule.count, m.cpu.cycles, m.irq_entries,
        m.chip.sample_count(), m.chip.dropped_samples(), m.cpu.pc, m.previous_dispatch, m.error};
}
} // namespace snd
