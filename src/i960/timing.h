// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Compiler-side cycle accounting from pinned MAME's i960 executor.
#pragma once
#include "decode.h"
#include <cstdint>
#include <optional>
namespace i960 {
// Fixed opcode deductions in MAME, not verified PCB timing. Unknown or
// non-executable instructions return nullopt. Call/callx/calls and ret return
// zero: shared Cpu helpers must separately charge 9/7 cycles, including IRQs.
// flushreg is also zero in the reference. Burst-stall continuation/time is
// outside this table; no branch/cache/alignment penalties are invented.
// This is used by the recompiler, never decoded in the shipping runtime.
std::optional<uint16_t> reference_opcode_cycles(const Insn& instruction);
} // namespace i960
