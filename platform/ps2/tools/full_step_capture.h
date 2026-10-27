#pragma once
// Host reference instrumentation; never a native game dependency.
#if defined(_EE) || defined(RALLY_PS2_EE)
#error "Whole-step capture is host-only"
#endif
#include "runtime/cpu.h"
#if defined(M2_FAST_GEN) || defined(M2_FAST_CPU) || defined(M2_FAST_BUS) || defined(M2_DC_SPEED)
#error "Whole-step capture requires every CPU data access to use Bus"
#endif
#include <array>
#include <cstdint>

namespace ps2::offline {
// Snapshot reads must still reach the original bus, but are not game accesses.
struct SuppressStepAccess {
    SuppressStepAccess();
    ~SuppressStepAccess();
    SuppressStepAccess(const SuppressStepAccess &) = delete;
    SuppressStepAccess &operator=(const SuppressStepAccess &) = delete;
};
void begin_full_step_capture(const char *directory);
void full_step_controls(std::uint64_t frame, const std::array<std::uint8_t,12> &controls);
void full_step_instruction(std::uint64_t instructions);
void full_step_boundary(const rt::Cpu &cpu,const char *event,std::uint64_t id,
                        std::uint32_t object,std::uint64_t instructions,
                        std::uint64_t frame,std::uint64_t ns);
void end_full_step_capture();
void full_step_access(const rt::Cpu *cpu,std::uint32_t address,std::uint32_t value,
                      unsigned width,bool writing);

// Forward each original operation exactly once, preserving width, burst flags,
// scenery access and exceptions. Instruction fetches are deliberately separate
// from data dependencies; they still reach the underlying implementation.
class StepCaptureBus final : public rt::Bus {
    rt::Bus &underlying_;
    const rt::Cpu *cpu_=nullptr;
public:
    explicit StepCaptureBus(rt::Bus &bus):underlying_(bus) {}
    void bind(const rt::Cpu &cpu) { cpu_=&cpu; }
    std::uint32_t fetch(std::uint32_t a) override { return underlying_.fetch(a); }
    std::uint8_t read_byte(std::uint32_t a) override {
        const auto v=underlying_.read_byte(a);full_step_access(cpu_,a,v,1,false);return v;
    }
    std::uint16_t read_word(std::uint32_t a) override {
        const auto v=underlying_.read_word(a);full_step_access(cpu_,a,v,2,false);return v;
    }
    std::uint32_t read_dword(std::uint32_t a) override {
        const auto v=underlying_.read_dword(a);full_step_access(cpu_,a,v,4,false);return v;
    }
    void write_byte(std::uint32_t a,std::uint8_t v) override {
        underlying_.write_byte(a,v);full_step_access(cpu_,a,v,1,true);
    }
    void write_word(std::uint32_t a,std::uint16_t v) override {
        underlying_.write_word(a,v);full_step_access(cpu_,a,v,2,true);
    }
    void write_dword(std::uint32_t a,std::uint32_t v) override {
        underlying_.write_dword(a,v);full_step_access(cpu_,a,v,4,true);
    }
    std::uint16_t flags(std::uint32_t a) override { return underlying_.flags(a); }
    rt::SceneryState *scenery() override { return underlying_.scenery(); }
};
} // namespace ps2::offline
