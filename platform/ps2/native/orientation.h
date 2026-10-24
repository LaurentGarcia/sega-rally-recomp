#pragma once

namespace ps2::native {

// Axis order is not yet established; these names preserve channel order only.
struct OrientationAngles {
    float first;
    float middle;
    float third;
};

// Fold each channel once, then bound the first and third to signed pi/8.
// Requires all inputs within +/-round32(2*pi), including signed zero.
// Returns false without changing any channel if any input is outside that
// domain or nonfinite. This restriction belongs to the native interface.
bool finalize_orientation(OrientationAngles &angles) noexcept;

} // namespace ps2::native
