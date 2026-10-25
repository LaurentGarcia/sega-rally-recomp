#pragma once

namespace ps2::native {
struct MetricPoint3 { float x = 0, y = 0, z = 0; };

// Recovered scalar operation, preserving the pinned reference's separate
// binary32 rounding and correction steps. This is not a correctly rounded
// Euclidean distance. Inputs must be finite in [-2^20,2^20]; the ordered sum
// of rounded squared differences must be zero or normal. Signed input zeros
// and subnormals are supported within that domain. Failure leaves output
// unchanged. Output may alias any input coordinate, even when inputs coincide.
[[nodiscard]] bool recovered_scalar_metric(const MetricPoint3 &first,
                                           const MetricPoint3 &second,
                                           float &output) noexcept;
} // namespace ps2::native
