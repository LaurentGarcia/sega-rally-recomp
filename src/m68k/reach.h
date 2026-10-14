// Recursive-descent reachability over the sound CPU's 68000 program, as for
// the i960 (src/i960/reach.h): follows direct control flow; each indirect
// jmp / jsr is recorded as a site, its targets coming from the MAME harvest
// or the seeds file.
#pragma once

#include "m68k/decode.h"

#include <cstdint>
#include <map>
#include <vector>

namespace m68k {

struct ReachResult {
    std::map<uint32_t, Insn> insns;        // every reachable instruction, by address
    std::vector<uint32_t> indirect_sites;  // jmp / jsr through a register
    std::vector<uint32_t> stops;           // paths that ran into an undefined opcode
    std::vector<uint32_t> unmapped;        // targets outside the image
};

ReachResult reach(const std::vector<uint32_t> &seeds, const ReadWord &read);

// Seeds from the vector table: the reset PC and every exception / interrupt
// vector (2-63) that points into the image, unique.
// A short reset copy may expose fewer vectors. This is discovery metadata;
// it does not initialize or alter guest RAM.
std::vector<uint32_t> vector_seeds(const ReadWord &read, uint32_t table_address = 0,
                                 uint32_t vector_count = 64);

} // namespace m68k
