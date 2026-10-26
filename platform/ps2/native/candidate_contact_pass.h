#pragma once
#include "contact_pass_response.h"
#include "rounded_affine_transform.h"
#include <span>
namespace ps2::native {
// Immutable owner-supplied native course/normal/local-point data. No original
// addresses or command transport survive this typed boundary.
struct CandidateContactPassInput {
    std::span<const ContactSegment> course;
    std::span<const ContactNormalPair> normal_table;
    std::array<AffinePoint3,4> local_points{};
    std::int16_t segment=0;
    float response_parameter=1, gain_parameter=1, scalar_divisor=1;
    std::array<float,3> bases{};
    bool feedback_enabled=false;
};
struct CandidateContactPassState {
    ContactPassResponseState response{};
    // A status-only miss retains each slot's earlier direction/gap bits.
    std::array<ContactResult,4> contacts{};
};
// Use candidate pose to prepare four points, search and interpret each contact,
// then resolve the complete response. Constituent numerical and asset bounds
// apply. Any rejection preserves every state field. This operation has no
// allocation, platform calls, original processors or device state.
[[nodiscard]] bool resolve_candidate_contact_pass(const CandidateContactPassInput& input,
                                                  CandidateContactPassState& state) noexcept;
} // namespace ps2::native
