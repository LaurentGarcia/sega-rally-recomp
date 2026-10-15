// Same-Machine reproducibility only; no independent CPU, MAME, or PCB claim.
#pragma once
#include "host_input_transcript.h"
#include <filesystem>
#include <memory>
#include <span>

namespace srally::recording {
using Machine = rt::Model2AMachine;
using Witness = host_input::Witness;
enum class TerminalKind { Quit, FrameLimit, InstructionLimit, Frontier, Fault };
const char *terminal_name(TerminalKind);
struct StartupIdentity {
    struct Data;
private:
    std::shared_ptr<const Data> data_;
    friend StartupIdentity identify_startup(const Machine::Images &, Machine::Options,
        std::span<const uint8_t>, const std::filesystem::path &);
    friend class Recorder;
};
// Computes identities from actual loaded bytes and the actual executable file.
// assets.json is opaque identity_only source evidence, not authentication of
// the upstream generation. No caller-supplied digest is accepted here.
StartupIdentity identify_startup(const Machine::Images &images, Machine::Options options,
    std::span<const uint8_t> assets_report, const std::filesystem::path &executable);
class Recorder {
public:
    Recorder(const std::filesystem::path &directory, StartupIdentity identity, const Machine &initial);
    ~Recorder();
    // Never runs, synchronizes, or drains a Machine. Pass the PCM from the
    // host's existing one drain after this exact resume; raw float bits count.
    void append(const host_input::Resume &, const Machine::RunResult &, std::span<const float> pcm);
    void finish(const Machine &terminal, TerminalKind);
    uint64_t records() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
struct ReplaySummary { uint64_t records; Witness terminal; bool frontier; TerminalKind kind; };
// Preflight the WHOLE bundle before creating/executing the replay Machine.
// Hash actual executable/assets/images, load only captured startup seeds,
// preserve all ten options, drain once per resume, and compare all evidence.
// No SDL/GPU/audio output, pacing, or writes to bundle/data/live-save paths.
ReplaySummary replay_bundle(const std::filesystem::path &bundle,
    const std::filesystem::path &data, const std::filesystem::path &actual_executable);
std::vector<uint8_t> read_bounded(const std::filesystem::path &, uint64_t maximum_bytes);
} // namespace srally::recording
