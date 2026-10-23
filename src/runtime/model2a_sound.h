// Optional instruction-boundary sound board for diagnostic machine bring-up.
// This does not model 68000 bus phases or value-bearing instruction prefetch.
#pragma once
#include "runtime/model2_scsp.h"
#include <memory>
#include <string>

namespace snd {
class Model2ASound {
public:
    struct Images {
        std::vector<uint8_t> program, samples;
        SoundImageSource *rom = nullptr; // Host-owned; must outlive this board.
    };
    enum class Status { Running, MissingCode, RuntimeError };
    struct Stats {
        Status status = Status::Running;
        uint64_t instructions = 0, cycles = 0, irq_entries = 0;
        uint64_t sample_frames = 0, dropped_samples = 0;
        uint32_t pc = 0, previous_dispatch = 0;
        std::string error;
    };
    // Shared clock and random-provider state must outlive this board.
    // Every generated instruction executes at its start boundary; its cycle
    // total schedules the next callback. MMIO is not timed to individual bus
    // transfers. One callback handles either an IRQ entry or an instruction.
    Model2ASound(rt::Model2AClock &clock, Images images,
                Model2Scsp::RandomProvider random, size_t buffered_frames = Model2Scsp::SAMPLE_HZ);
    ~Model2ASound();
    Model2ASound(const Model2ASound &) = delete;
    Model2ASound &operator=(const Model2ASound &) = delete;
    std::function<void(bool)> on_midi_tx;
    void midi_in(bool level);
    std::vector<Model2Scsp::Sample> take_samples();
    Stats stats() const;
    const Cpu68k &cpu() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace snd
