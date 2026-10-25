#pragma once

#include "orientation.h"

namespace ps2::native {

struct AlternateOrientationUpdate {
    OrientationAngles folded_candidate;
    OrientationAngles orientation;
};

// Fold the candidate once, then apply the recovered wrapped-difference
// smoothing/step rule. Channel names do not identify physical axes or units.
// Both input triples must be within +/-round32(2*pi); each rounded difference
// current-folded_candidate must also fit that range. Other inputs return false
// without changing output. All rounding is nearest/even, including binary64
// intermediate rounding. Inputs may alias either member of output.
[[nodiscard]] bool smooth_alternate_orientation(
    const OrientationAngles &current, const OrientationAngles &candidate,
    AlternateOrientationUpdate &output) noexcept;

} // namespace ps2::native
