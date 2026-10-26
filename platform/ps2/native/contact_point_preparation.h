#pragma once
#include "rounded_affine_transform.h"
#include <array>

namespace ps2::native {
struct ContactPointPreparation {
    AffineTransform3 matrix{};
    std::array<AffinePoint3,4> offsets{};
    std::array<AffinePoint3,4> points{};
};

// Recovered, rounded orientation construction and four contact-point transforms.
// Orientation components are finite in [-8,8]; position/local coordinates are
// finite in [-2^20,2^20], including signed zero and subnormals. The orientation
// uses the recovered polynomial, not a platform sin/cos approximation. Components
// retain their original first/middle/third order; physical axis names are unproven.
// Every intermediate binary32 operation rounds separately to nearest/even.
// On rejection output is unchanged. Input references may alias output members.
bool prepare_contact_points(const AffinePoint3 &orientation,
                            const AffinePoint3 &position,
                            const std::array<AffinePoint3,4> &local_points,
                            ContactPointPreparation &output) noexcept;
} // namespace ps2::native
