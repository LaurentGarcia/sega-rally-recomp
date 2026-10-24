#pragma once

namespace ps2::native {

// Component order only: physical axes and normalization are not established.
struct PlanarPair {
    float first;
    float third;
};

// Independently recovered planar blend. Accepts binary32 components in [-1,1],
// including signed zero/subnormal encodings. Rejects other values atomically.
// Output may alias either input. It is not normalized or clamped to [-1,1].
// Arithmetic is nearest/even at each observed boundary, independent of the
// host FPU. Exception flags and other rounding modes are outside this contract.
bool blend_planar_direction(const PlanarPair &base, const PlanarPair &target,
                            PlanarPair &output) noexcept;

} // namespace ps2::native
