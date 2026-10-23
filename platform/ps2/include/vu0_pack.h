#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace ps2 {
// Two vertices, already scaled to the output viewport and translated by the
// GS XY origin. Each component must be finite and in [0,4096). In particular,
// add the floating-point origin BEFORE conversion: trunc(x*16)+origin*16
// differs from trunc((x+origin)*16) for negative screen positions.
struct alignas(16) GsCoordinates { float xy[4]; };
struct alignas(16) GsFixedCoordinates { std::uint32_t xy[4]; };
static_assert(sizeof(GsCoordinates) == 16 && sizeof(GsFixedCoordinates) == 16);

// Portable scalar reference, with the same preconditions as the VU kernel.
// Inputs/outputs must not overlap, and output must contain at least count
// elements. No projection, clipping, color, depth or texture arithmetic occurs.
void quantize_gs_xy_scalar(const GsCoordinates *input, GsFixedCoordinates *output,
                           std::size_t count) noexcept;

// EE macro-mode VU0 kernel (portable builds use the scalar reference). The
// rendering thread exclusively owns VU0 while this synchronous call runs;
// do not overlap it with a VU0 microprogram or another macro-mode user.
// Four float-to-fixed lanes run per VFT0I4, without reciprocal approximations.
// This is opt-in until hardware measurements establish a speed benefit.
void quantize_gs_xy_vu0(const GsCoordinates *input, GsFixedCoordinates *output,
                        std::size_t count) noexcept;
bool vu0_pack_available() noexcept;

// Validate the complete batch before writing anything. Rejects a short output,
// overlapping arrays and invalid coordinates; an empty batch is successful.
bool quantize_gs_xy_checked(std::span<const GsCoordinates> input,
                            std::span<GsFixedCoordinates> output,
                            bool use_vu0) noexcept;
} // namespace ps2
