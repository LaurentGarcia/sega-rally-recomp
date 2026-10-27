#pragma once
#include "candidate_contact_pass.h"
#include "main_candidate.h"

namespace ps2::native {
struct MainContactStepInput {
    std::span<const ContactSegment> course;
    std::span<const ContactNormalPair> normal_table;
    std::array<AffinePoint3,4> local_points{};
    std::int16_t segment=0;
    float response_parameter=1, gain_parameter=1, scalar_divisor=1;
    bool feedback_enabled=false;
};
struct MainContactStepState {
    MainCandidateState vehicle{};
    std::array<std::int32_t,2> feedback{};
    std::uint32_t contact_mask=0;
    float selected_scalar=0;
    std::array<ContactResult,4> contacts{};
};
// Compose candidate preparation and its immediately following four-point
// contact pass. Bases and candidate/motion inputs are derived from the newly
// prepared vehicle, never supplied as contradictory caller values. This is
// one region of the main update, not a complete vehicle tick. Constituent
// numerical/asset domains apply; failure preserves the entire state.
[[nodiscard]] bool resolve_main_contact_step(const MainContactStepInput& input,
                                             MainContactStepState& state) noexcept;
} // namespace ps2::native
