#pragma once
#include "contact_aggregate.h"
#include "contact_result.h"
#include <array>
#include <cstdint>
namespace ps2::native {
struct ContactPassResponseInput {
    std::array<ContactResult,4> contacts{};
    std::array<ContactVertex,4> offsets{};
    float response_parameter=1, gain_parameter=1, scalar_divisor=1;
    std::array<float,3> bases{};
    bool feedback_enabled=false;
};
struct ContactPassResponseState {
    ContactAggregateState car{};
    std::uint32_t contact_mask=0;
    float selected_scalar=0;
};
// Resolve four already-interpreted contacts in original index order. Each
// constituent's bounded arithmetic contract must hold when that path executes.
// The zero selected-scalar gate preserves the complete car state, even when
// unused fields contain arbitrary encodings. Output mask/scalar start fresh.
// Any failure leaves every state field unchanged. No query, transform, asset
// loading, hardware state or original processor executes here.
[[nodiscard]] bool resolve_contact_pass_response(const ContactPassResponseInput& input,
                                                 ContactPassResponseState& state) noexcept;
} // namespace ps2::native
