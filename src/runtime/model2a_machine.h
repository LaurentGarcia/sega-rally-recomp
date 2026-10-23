// Shared native Model 2A machine for bounded bring-up and SDL3/Metal hosts.
// Game code is generated separately from verified user ROMs.
#pragma once
#include "runtime/model2a_bus.h"
#include "runtime/model2a_io.h"
#include <memory>
#include <string>
#include <vector>

namespace rt {
class Video;
class Model2AMachine {
public:
    struct Images {
        std::vector<uint8_t> program, main_data, tgp_tables, copro_data, polygons, textures;
        std::vector<uint8_t> sound_program, sound_samples;
#ifdef M2_PAGED_ROM
        // Optional host-owned ROM cache. Program/data/geometry/copro images
        // use it; small TGP tables and sound images retain their own contract.
        RomSource *rom = nullptr;
#endif
    };
    struct Options {
        bool tgp = true, io = true, comm = true, control = true;
        bool video = true, screen = true, uart = true, external_video = false;
        bool sound = false; // Diagnostic 68000 instruction-boundary timing.
        // Requires an enabled runtime and separately verified generated chunks.
        // False retains the original one-instruction outer loop for comparison.
        bool instruction_boundary_hook = false;
    };
    enum class Reason { Frame, Budget, MissingCode, DeviceAccess, RuntimeError };
    struct RunResult {
        Reason reason = Reason::Budget;
        uint32_t pc = 0;
        uint64_t instructions = 0;
        std::string message;
        uint32_t address = 0, value = 0;
        unsigned width = 0;
        bool write = false;
    };
    struct Stats {
        uint64_t instructions = 0, reference_cycles = 0, board_time_ns = 0;
        uint64_t frames_rendered = 0, frames_parsed = 0, screen_frame = 0, latched_frame = 0;
        uint64_t tgp_instructions = 0, tx_pin_changes = 0, screen_hash = 0;
        uint64_t polygons = 0, frames_with_geometry = 0;
        uint64_t first_geometry_frame = 0; // one-based rendered frame; zero means none
        uint64_t sound_instructions = 0, sound_cycles = 0, sound_irqs = 0;
        uint64_t sound_samples = 0, sound_dropped_samples = 0;
        uint32_t sound_pc = 0, sound_previous_dispatch = 0;
        uint32_t pc = 0, previous_dispatch = 0;
        unsigned irq_entries = 0;
        bool tgp_booted = false;
        uint8_t uart_mode = 0, uart_command = 0;
    };
    Model2AMachine(Images images, Options options);
    ~Model2AMachine();
    Model2AMachine(const Model2AMachine &) = delete;
    Model2AMachine &operator=(const Model2AMachine &) = delete;
    RunResult run_for(uint64_t instruction_budget);
    RunResult run_until_frame(uint64_t instruction_budget);
    Stats stats(bool hash_screen = false) const;
    // Interleaved normalized float stereo, left then right, at 44,100 Hz.
    // Drains the bounded chip buffer (at most one second); never runs CPUs,
    // advances board time or changes IRQ state. Empty when sound is disabled.
    static constexpr unsigned kAudioRate = 44100;
    std::vector<float> take_audio();
    const Cpu &cpu() const;
    const Model2ABus::Diagnostics &diagnostics() const;
    Video &video();
    Model2AIO::Inputs &inputs();
    std::vector<uint8_t> save_eeprom() const;
    void load_eeprom(const std::vector<uint8_t> &image);
    std::vector<uint8_t> save_backup() const;
    void load_backup(const std::vector<uint8_t> &image);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rt
