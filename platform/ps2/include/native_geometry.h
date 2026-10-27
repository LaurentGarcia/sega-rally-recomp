#pragma once
#include "native_display.h"

namespace ps2::native {
// Reference platform operation, not recovered game projection or physics.
// Clip coordinates are y-up: -w <= x,y <= w and 0 <= z <= w. The independent
// positive-w guard prevents division at/behind the eye. This guard is not a
// claim about a game's near distance or coordinate units.
inline constexpr float clip_min_w = 0x1p-10f;
inline constexpr float clip_component_limit = 0x1p16f;
inline constexpr std::size_t clip_polygon_capacity = 10;
inline constexpr std::size_t clip_triangle_capacity = 8;

struct ClipVertex { float x = 0, y = 0, z = 0, w = 1, u = 0, v = 0; };
struct ClipTriangle {
    std::array<ClipVertex, 3> vertices;
    const Material *material = nullptr;
};
enum class ClipStatus {
    success,
    invalid_material,
    invalid_vertex,
    insufficient_capacity,
    numerical_failure,
};
struct ClipResult {
    ClipStatus status = ClipStatus::success;
    std::size_t count = 0;    // Number of output triangles written.
    std::size_t required = 0; // Exact count on success/capacity failure, else 0.
};

// Allocation-free, synchronous reference clipping. All six input components
// must be normal binary32 values or signed zero, with absolute value <= 65536.
// NaNs, infinities and subnormals are rejected by their encodings before math.
// Intersections/projecting use binary64 scratch arithmetic, measured separately
// by WorldScene's stage clock. This reference clipper does not execute on a VU.
//
// Plane order: positive w, left, right, bottom, top, near z, far z. Seven
// half-planes can grow a convex triangle to at most ten vertices/eight fans.
// UVs are interpolated as raw texel coordinates before q=1/w is calculated.
// Output maps to the y-down 496x384 logical viewport, with no half-texel bias.
// Optional native reverse depth is 1-z/w (far0/near1); it does not impose
// recovered arcade priority and only affects Frame::depth_test submissions.
// Material identity and fan order are preserved; there is no face sorting or
// backface culling. Exactly zero projected-area fans are omitted.
//
// Failure and zero-output success leave the entire output span unchanged.
// Capacity is checked against the actual surviving fan count, not the maximum.
// Tiny binary64 roundoff outside an already-processed plane may be snapped
// back; this tolerance is never used to accept an outside input vertex.
// Non-finite/subnormal final binary32 output is a numerical failure.
// Caller retains all materials/assets through the eventual Display::present.
ClipResult clip_project_triangle(const ClipTriangle &input,
                                 std::span<Triangle> output) noexcept;
} // namespace ps2::native
