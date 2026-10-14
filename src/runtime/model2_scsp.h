// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont, Carl, Miodrag Milanovic
#pragma once

#include "runtime/model2a_clock.h"
#include "runtime/model2a_sound_bus.h"
#include <functional>
#include <memory>
#include <vector>

namespace snd {

class Model2Scsp final : public Model2ASoundBus::Scsp {
public:
    static constexpr uint32_t CLOCK_HZ = 22'579'200, SAMPLE_HZ = 44'100;
    // Pinned running_machine::rand, not a claim about the physical noise RNG.
    // A whole-machine host should share the provider's state with other users.
    struct ReferenceRandom {
        uint32_t state = 0x9d14abd7;
        uint32_t operator()() {
            state = 1664525u * state + 1013904223u;
            return (state >> 16) | (state << 16);
        }
    };
    using RandomProvider = std::function<uint32_t()>;
    enum class LineState { Clear = 0, Assert = 1, Hold = 2 };
    struct Sample { float left = 0, right = 0; bool operator==(const Sample &) const = default; };

    // Clock, RAM, and any state captured by random must outlive this chip.
    // Capture is bounded; full buffers discard oldest samples and count them.
    // The host advances the shared clock before MMIO/physical pin changes.
    // Callbacks observe that queue's event deadline and may not advance time.
    // Output contains chip samples with current MVOL applied; a downstream
    // host resampler/mixer is separate from this 44.1 kHz component.
    Model2Scsp(rt::Model2AClock &clock, std::span<uint8_t> ram, RandomProvider random,
               size_t buffer_frames = SAMPLE_HZ);
    ~Model2Scsp();
    Model2Scsp(const Model2Scsp &) = delete;
    Model2Scsp &operator=(const Model2Scsp &) = delete;

    std::function<void(unsigned level, LineState state)> on_irq;
    std::function<void(bool level)> on_midi_tx;
    uint16_t read16(uint16_t byte_offset, uint16_t mask) override;
    void write16(uint16_t byte_offset, uint16_t data, uint16_t mask) override;
    void midi_in(bool level);
    // The reference resets serial framing flags/counters, preserving SCSP
    // registers, FIFO cursors, timer events, sample phase, shift data and clocks.
    void reset();
    void advance(uint64_t now_ns);
    uint64_t sample_count() const;
    uint64_t dropped_samples() const;
    std::vector<Sample> take_samples();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace snd
