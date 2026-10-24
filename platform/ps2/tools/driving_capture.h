#pragma once
// Offline observation only. Never linked into the native PS2 engine or game.
#if defined(_EE) || defined(RALLY_PS2_EE)
#error "Driving state capture belongs only in the host behavioral reference"
#endif
#include "runtime/cpu.h"
#include <cstdint>

namespace ps2::offline {
void begin_driving_capture(const char *directory, unsigned first_frame, unsigned last_frame);
void observe_driving_instruction(const rt::Cpu &cpu, std::uint64_t instructions,
                                 std::uint64_t frame, std::uint64_t board_ns);
void end_driving_capture();
} // namespace ps2::offline
