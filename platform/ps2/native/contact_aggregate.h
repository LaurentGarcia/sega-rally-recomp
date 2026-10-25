#pragma once
#include <array>
#include <cstdint>

namespace ps2::native {
struct ContactAggregateInput {
    float signed_scalar = 0;
    std::array<std::array<float,3>,4> responses{};
    // First candidate-position base, third position base, middle orientation base.
    std::array<float,3> bases{};
    float response_divisor = 1;
    float scalar_divisor = 1;
    bool feedback_enabled = false;
};
struct ContactAggregateState {
    // Storage order: first-position increment, third-position increment,
    // middle-orientation increment. These names do not assign physical units.
    std::array<float,3> motion{};
    std::array<float,3> candidate_position{};
    std::array<float,3> candidate_orientation{};
    std::array<std::int32_t,2> feedback{};
};

// Subtract metric, with its sign flipped only when sign_source is negative.
// Inputs are finite with magnitude <=2^20; metric must be nonnegative (either
// zero sign is accepted). Separately rounded original arithmetic is preserved.
// Rejection leaves output unchanged; output may alias any input float.
[[nodiscard]] bool accumulate_contact_scalar(float accumulator, float metric,
    float sign_source, float& output) noexcept;

// Apply an already-active aggregate path; the caller owns its earlier gate.
// Arithmetic inputs have magnitude <=2^20. Divisor magnitudes are within
// [2^-20,2^20], with either sign allowed. Response middle components and the
// preserved candidate fields are unused and may contain arbitrary bits.
// Enabled feedback additionally requires both truncated products to fit int32,
// and the first absolute value to fit positive int32. Disabled feedback is
// preserved exactly. Any rejection leaves the complete state unchanged.
[[nodiscard]] bool apply_contact_aggregate(const ContactAggregateInput& input,
    ContactAggregateState& state) noexcept;
} // namespace ps2::native
