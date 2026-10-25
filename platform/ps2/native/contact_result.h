#pragma once
#include "contact_search.h"
#include <array>
#include <span>

namespace ps2::native {
// Immutable owner-supplied interpolation coefficients, decoded offline. The
// 8192 pairs are ordinary native asset data, not processor or port state.
struct ContactNormalPair { float slope, intercept; };
struct ContactResult {
    std::array<float, 3> direction{};
    float gap = 0;
    std::uint32_t status = 0;
};

// Interpret a selected polygon's reply. Attribute zero only sets status 15;
// direction/gap remain unchanged. Other attributes set (attributes>>16)&0x8f.
// Bit7 of that status selects the edge branch (copied direction); the other
// branch computes a plane gap and table-interpolated direction.
// Values used in arithmetic must be finite in [-2^20,2^20]. The edge branch
// copies its three direction words verbatim, including nonfinite encodings.
// A used divisor may be negative and has magnitude >=2^-20.
// Plane squared length must be >=2^-40;
// its table has 8192 pairs, and the used slope/intercept must be in [1/8,1]
// and [1,4], respectively. Unused values/payload bits are not validated.
// Unsupported inputs leave every result field unchanged. No allocation or
// command transport; the caller's separate three-word output prefix is untouched.
[[nodiscard]] bool interpret_contact_result(const ContactPolygon& polygon,
    const ContactVertex& point, std::span<const ContactNormalPair> normal_table,
    ContactResult& result) noexcept;
} // namespace ps2::native
