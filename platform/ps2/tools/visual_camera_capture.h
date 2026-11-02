#pragma once
// Offline observation only; never a native game dependency.
#if defined(_EE) || defined(RALLY_PS2_EE)
#error "Visual camera capture belongs only in the host reference"
#endif
#include "runtime/cpu.h"
#include <cstdint>
namespace ps2::offline {
void begin_visual_camera_capture(const char* directory,unsigned first,unsigned last);
void observe_visual_camera(const rt::Cpu&,std::uint64_t instructions,
                           std::uint64_t frame,std::uint64_t board_ns);
void end_visual_camera_capture();
}
