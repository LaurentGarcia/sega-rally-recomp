#pragma once
#include "native_geometry_float.h"

namespace ps2::native::detail {
// Internal WorldScene cache contract, not an unchecked public draw API.
// Accepts the float clipper's normal-or-zero bounded domain and only vertices
// inside every clip plane. On success, result is the unchanged float clipper's
// inside projection. On failure, result is unspecified and must not be cached;
// the caller retains the original clip vertex for the complete triangle path.
bool project_inside_float(const ClipVertex&, Vertex& result) noexcept;

// Each vertex's position/q/depth must be an unmodified successful
// project_inside_float result. UV may be replaced with a face corner's already
// validated normal-or-zero texel coordinates (abs<=65536): projection copies
// those two values unchanged. This permits seams at a shared position index.
// Only WorldScene's private, per-instance cache and corner scratch retain this
// proof. Applies the original area/capacity/material checks and atomic commit.
// Outside or unsuccessfully projected corners must use the full float clipper.
ClipResult emit_projected_triangle(const std::array<Vertex,3>&,
                                  const Material*,std::span<Triangle>) noexcept;
}
