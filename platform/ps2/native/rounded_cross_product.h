#pragma once

namespace ps2::native {
struct CrossVector3 { float x = 0, y = 0, z = 0; };

// Componentwise nearest/even binary32 products, then a separately rounded
// subtraction for each cyclic determinant. Each input component must be finite
// and in [-2^20,2^20]; signed zeros and subnormals are supported. Invalid input
// leaves output unchanged. Output may alias either or both input objects.
// No normalization, epsilon, fused multiply/add or coordinate-system inference.
[[nodiscard]] bool rounded_cross_product(const CrossVector3 &left,
                                         const CrossVector3 &right,
                                         CrossVector3 &output) noexcept;
} // namespace ps2::native
