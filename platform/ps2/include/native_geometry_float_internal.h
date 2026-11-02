#pragma once
#include "native_geometry_float.h"

namespace ps2::native::detail {
// Trusted early rejection only: each x/y/z/w field is a normal binary32 value
// or signed zero with abs<=65536, already validated by the caller. These are
// the float clipper's exact seven strict comparisons, including signed w (not
// abs(w)). True proves zero output for otherwise valid material/UV inputs;
// false makes no claim about visibility. No public input validation is skipped.
// Position needs only float x/y/z/w fields: no UV construction or VU dependency.
template<class Position>
inline bool common_outside_validated(const Position& a, const Position& b,
                                     const Position& c) noexcept {
    const auto outside = [](const Position& p) {
        return (p.w < clip_min_w ? 1u : 0u) |
               (p.x < -p.w ? 2u : 0u) | (p.x > p.w ? 4u : 0u) |
               (p.y < -p.w ? 8u : 0u) | (p.y > p.w ? 16u : 0u) |
               (p.z < 0.0f ? 32u : 0u) | (p.z > p.w ? 64u : 0u);
    };
    const unsigned ab = outside(a) & outside(b);
    return ab != 0 && (ab & outside(c)) != 0;
}

// Internal WorldScene cache contract, not an unchecked public draw API.
// Accepts the float clipper's normal-or-zero bounded domain and only vertices
// inside every clip plane. On success, result is the unchanged float clipper's
// inside projection. On failure, result is unspecified and must not be cached;
// the caller retains the original clip vertex for the complete triangle path.
bool project_inside_float(const ClipVertex&, Vertex& result) noexcept;

// Each vertex's position/q/depth must be a successful project_inside_float
// result or a WorldScene-validated native projection-provider result within
// that callback's numerical allowance. UV may be replaced with a face corner's already
// validated normal-or-zero texel coordinates (abs<=65536): projection copies
// those two values unchanged. This permits seams at a shared position index.
// Only WorldScene's private, per-instance cache and corner scratch retain this
// proof. Applies the original area/capacity/material checks and atomic commit.
// Outside or unsuccessfully projected corners must use the full float clipper.
ClipResult emit_projected_triangle(const std::array<Vertex,3>&,
                                  const Material*,std::span<Triangle>) noexcept;
}
