#pragma once
#include "native_geometry.h"

namespace ps2::native {
// Native binary32 rendering path. This uses the same seven clipping planes,
// texel UVs, reverse depth and 496x384 viewport as clip_project_triangle, but
// all intersection, projection and area arithmetic is float, including on EE.
// It is not a recovered arcade projection or a bit-identical reference path.
//
// Each input component must be normal binary32 or signed zero and |v|<=65536;
// w is clipped to at least 1/1024 before division. Interpolated vertices may
// receive at most 16 binary32 epsilons of scale-relative plane repair, only
// after strict clipping. No tolerance admits an originally outside vertex.
// Large cancellation, underflow and rounding can change coverage or make a
// projected fan degenerate. There is no universal pixel-error guarantee for
// ill-conditioned input, and no reference/double fallback in this function.
//
// All returned components are finite normal-or-zero, screen/depth/q bounds
// hold, and material identity/fan order are retained. Numerical or capacity
// failure (and empty success) leaves the complete output span unchanged.
// At most ten polygon vertices/eight triangles are staged on the stack;
// required is the actual surviving count on a capacity failure.
ClipResult clip_project_triangle_float(const ClipTriangle &input,
                                       std::span<Triangle> output) noexcept;
} // namespace ps2::native
