// Cycle plans for statically decoded 68000 instructions. This is accounting
// at instruction boundaries, not a bus-phase or prefetch-value simulation.
#pragma once
#include "m68k/decode.h"
#include <optional>

namespace m68k {
enum class TimingDynamic : uint8_t { None, Branch, Dbcc, Scc, Shift, MultiplyUnsigned, MultiplySigned, BitRegister };
enum class TimingFetch : uint8_t { Sequential, Branch, Dbcc, Jump, Return, Status };
struct TimingPlan {
    uint16_t cycles=0; // nominal core cycles, before mapped waits/dynamic additions
    TimingDynamic dynamic=TimingDynamic::None;
    TimingFetch fetch=TimingFetch::Sequential;
    bool dummy_destination_read=false;
    bool movem_trailing_read=false;
};
// nullopt is an explicit timing frontier, never a guessed default cost.
std::optional<TimingPlan> timing_plan(const Insn &instruction);
} // namespace m68k
