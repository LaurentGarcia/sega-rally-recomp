// Test oracle only. No production bus, CPU, scheduler or replay flag changes.
#pragma once
#include "host_input_transcript.h"
#include "refcore/i960_ref.h"
#include "i960/decode.h"
#include "runtime/model2_eeprom.h"
#include <array>
#include <span>

namespace srally::reference_resume {
using Machine = rt::Model2AMachine;
using Witness = host_input::Witness;

// The caller obtains these from the audited generated instruction inventory
// and actual verified program/reset-copy bytes. This is NOT a decoder cache
// over arbitrary MMIO or a permission to ignore a failed live-word guard.
struct GuardedInstruction {
    std::array<uint32_t, 2> words{};
    unsigned count = 0;
};
struct Hooks {
    std::function<void()> synchronize;
    // Drain queued edges, then check_immediate_irqs, once per preparation.
    std::function<void()> interrupts;
    std::function<uint64_t()> board_time_ns, frames;
    std::function<std::optional<GuardedInstruction>(uint32_t)> guarded_instruction;
    rt::Model2AIO::Inputs *inputs = nullptr;
};

inline void load_startup_seeds(rt::Model2Eeprom &eeprom, rt::Model2ABus &bus,
        std::span<const uint8_t> initial_eeprom, std::span<const uint8_t> initial_backup) {
    // Validate BOTH sizes before changing either independent board.
    if (initial_eeprom.size() != 128 || initial_backup.size() != 16384)
        throw rt::Fatal("reference resume: unsupported startup seed sizes");
    if (!eeprom.load_image(initial_eeprom)) throw rt::Fatal("reference resume: invalid EEPROM seed");
    bus.load_backup({initial_backup.begin(), initial_backup.end()});
}

// Exact synchronization-point adapter, NOT hardware instruction-fetch parity.
// The measured long-LDA discrepancy is 0ns (Machine) versus 40ns (unaltered
// interpreter). Relocated code guards read both words BEFORE charging. The
// adapter preserves those actual RAM guard bytes/faults/order. For immutable
// ROM code the native emitter embeds words without bus guards: prefetch here
// is explicitly oracle byte validation/decoding, not a native/hardware fetch
// equivalence claim. Every data read/write/flags access still goes to the real
// bus at its original reference-cycle instant.
class GuardPhaseBus final : public rt::Bus {
public:
    explicit GuardPhaseBus(rt::Bus &bus) : bus_(bus) {}
    void execute(rt::I960Ref &cpu, const GuardedInstruction &instruction) {
        if (instruction.count < 1 || instruction.count > 2 ||
                (cpu.m_IP & 3) || cpu.m_IP > UINT32_MAX - (instruction.count - 1) * 4)
            throw rt::Fatal("reference resume: unproven instruction-fetch profile");
        const auto bytes = instruction.count * 4;
        // Model2ABus's program map or the verified reset-work-RAM copy only.
        // In particular, never cache instruction-shaped reads from MMIO.
        if (!((cpu.m_IP < 0x200000 && bytes <= 0x200000 - cpu.m_IP) ||
                (cpu.m_IP >= 0x5a0000 && cpu.m_IP < 0x600000 && bytes <= 0x600000 - cpu.m_IP)))
            throw rt::Fatal("reference resume: unproven executable memory range");
        const auto decoded = i960::decode(cpu.m_IP,instruction.words[0],instruction.words[1]);
        if (!decoded.executable() || decoded.length != bytes)
            throw rt::Fatal("reference resume: unproven instruction length/decode");
        pc_ = cpu.m_IP; count_ = instruction.count; next_ = 0;
        for (unsigned n = 0; n < count_; ++n) {
            cached_[n] = bus_.fetch(pc_ + 4 * n);
            if (cached_[n] != instruction.words[n])
                throw rt::Fatal("reference resume: live instruction word differs from verified guard");
        }
        active_ = true;
        try {
            cpu.m_icount = 0;
            cpu.execute_one();
            active_ = false;
            if (next_ != count_) throw rt::Fatal("reference resume: instruction-fetch profile was not consumed");
        } catch (...) { active_ = false; throw; }
    }
    uint32_t fetch(uint32_t address) override {
        if (!active_) return bus_.fetch(address);
        if (next_ >= count_ || address != pc_ + next_ * 4)
            throw rt::Fatal("reference resume: unexpected oracle instruction fetch");
        return cached_[next_++];
    }
    uint8_t read_byte(uint32_t a) override { return bus_.read_byte(a); }
    uint16_t read_word(uint32_t a) override { return bus_.read_word(a); }
    uint32_t read_dword(uint32_t a) override { return bus_.read_dword(a); }
    void write_byte(uint32_t a, uint8_t v) override { bus_.write_byte(a,v); }
    void write_word(uint32_t a, uint16_t v) override { bus_.write_word(a,v); }
    void write_dword(uint32_t a, uint32_t v) override { bus_.write_dword(a,v); }
    uint16_t flags(uint32_t a) override { return bus_.flags(a); }
private:
    rt::Bus &bus_;
    std::array<uint32_t,2> cached_{};
    uint32_t pc_ = 0;
    unsigned count_ = 0, next_ = 0;
    bool active_ = false;
};

// SOUND OFF ONLY. This low-level adapter accepts preflighted ordered rows, not
// filenames or externally supplied digest assertions. Before construction the
// caller validates the WHOLE immutable sealed bundle, actual host identity,
// linked generated/profile inventory, images, seeds and ALL supported options.
// A true capability flag alone does not perform that validation. Unknown board
// configurations and coupled sound must stay rejected by the integration.
class Stepper {
public:
    Stepper(rt::I960Ref &cpu, Hooks hooks, const host_input::Context &context,
            host_input::IndependentCpuCapabilities capabilities)
        : cpu_(cpu), hooks_(std::move(hooks)), original_bus_(cpu.bus), fetch_bus_(*cpu.bus) {
        host_input::require_independent_cpu_support(context, capabilities);
        if (!hooks_.synchronize || !hooks_.interrupts || !hooks_.board_time_ns || !hooks_.frames ||
                !hooks_.guarded_instruction || !hooks_.inputs)
            throw rt::Fatal("reference resume: incomplete independent board hooks");
        cpu_.bus = &fetch_bus_;
    }
    ~Stepper() { cpu_.bus = original_bus_; }
    Stepper(const Stepper &) = delete;
    Stepper &operator=(const Stepper &) = delete;
    Witness witness() const {
        return {count_,cpu_.reference_cycles,hooks_.board_time_ns(),hooks_.frames(),cpu_.m_IP};
    }
    Machine::RunResult run_until_frame(uint64_t budget, const rt::Model2AIO::Inputs &inputs) {
        if (poisoned_) throw rt::Fatal("reference resume: adapter is poisoned");
        if (stopped_) return *stopped_;
        try {
            if (!budget || budget > 100000 || budget > UINT64_MAX - count_)
                throw rt::Fatal("reference resume: invalid instruction budget");
            *hooks_.inputs = inputs; // Exactly once per actual public resume.
            const auto target = count_ + budget, first_frame = hooks_.frames();
            for (;;) {
                // Frame precedes an exact Budget tie, with no added end sync.
                if (hooks_.frames() != first_frame) return result(Machine::Reason::Frame);
                if (count_ >= target) return result(Machine::Reason::Budget);
                hooks_.synchronize(); hooks_.interrupts();
                // Complete IRQ entry before Frame, even with unchanged count.
                if (hooks_.frames() != first_frame) return result(Machine::Reason::Frame);
                const auto instruction = hooks_.guarded_instruction(cpu_.m_IP);
                if (!instruction) {
                    stopped_ = result(Machine::Reason::MissingCode,"native code target is not compiled");
                    return *stopped_;
                }
                fetch_bus_.execute(cpu_,*instruction);
                ++count_;
            }
        } catch (...) { poisoned_ = true; throw; }
    }
    Machine::RunResult consume(const host_input::Resume &row) {
        if (poisoned_) throw rt::Fatal("reference resume: adapter is poisoned");
        try {
            if (row.index != row_ || row.before != witness())
                throw rt::Fatal("reference resume: ordered pre-witness differs");
            if (row.reason == Machine::Reason::DeviceAccess || row.reason == Machine::Reason::RuntimeError)
                throw rt::Fatal("reference resume: faulting instruction phase is not yet supported");
            const auto outcome = run_until_frame(row.budget,row.inputs);
            if (row.after != witness() || row.reason != outcome.reason)
                throw rt::Fatal("reference resume: ordered post-witness/reason differs");
            ++row_; return outcome;
        } catch (...) { poisoned_ = true; throw; }
    }
private:
    Machine::RunResult result(Machine::Reason reason, std::string message = {}) const {
        return {reason,cpu_.m_IP,count_,std::move(message)};
    }
    rt::I960Ref &cpu_;
    Hooks hooks_;
    rt::Bus *original_bus_;
    GuardPhaseBus fetch_bus_;
    uint64_t count_ = 0, row_ = 0;
    bool poisoned_ = false;
    std::optional<Machine::RunResult> stopped_;
};
} // namespace srally::reference_resume
