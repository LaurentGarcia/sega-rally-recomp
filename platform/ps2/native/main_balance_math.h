#pragma once
#include "rounded_affine_transform.h"

namespace ps2::native {
// Recovered geometry polynomial and a single middle-axis matrix rotation.
// These are value operations, not a geometry-command processor. Angles have
// magnitude <=16; the affine primitive's input/output domain also applies.
bool main_balance_sine(float angle,float& output) noexcept;
bool main_balance_rotate(float angle,const AffinePoint3& input,
                         AffinePoint3& output) noexcept;
} // namespace ps2::native
