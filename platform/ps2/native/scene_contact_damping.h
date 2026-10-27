#pragma once
#include "rounded_affine_transform.h"
namespace ps2::native {
struct SceneContactGeometry {
    AffinePoint3 published_point{}, reference_point{}, direction{};
};
// Scalar correction used by the scene-relative branch. Coordinates/direction
// and base are finite within ±2^20. The rounded dot <= 0 branch copies base bits.
// A positive dot uses the recovered metric's exact input ordering/domain;
// its squared result and quotient must be finite, with a nonzero denominator.
// Upper clamp is 1; no lower clamp. Unsupported input leaves output unchanged.
// This computes a damping value, not geometry ownership or game-state motion.
[[nodiscard]] bool correct_scene_contact_damping(const SceneContactGeometry&,
                                                float base, float& output) noexcept;
} // namespace ps2::native
