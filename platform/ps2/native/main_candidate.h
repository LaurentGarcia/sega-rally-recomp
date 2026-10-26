#pragma once
#include <array>
#include <cstdint>

namespace ps2::native {
struct MainCandidateState {
    // Storage roles only: no physical axes, units or fixed time step assumed.
    std::array<float,3> position{}, pending_position_delta{}, motion{};
    std::array<float,3> orientation{}, angular_motion{};
    std::uint32_t response_marker=0;
    std::array<float,3> candidate_position{}, candidate_orientation{};
};

// Clamp the pending middle component, evaluate the recovered metric gate,
// conditionally consume the pending delta, and form both candidate triples.
// The fifteen input components are finite in [-2^20,2^20]. The metric's ordered
// squared sum must be zero or normal. Existing candidate values are unused.
// No allocation, device state or original addresses; rejection is atomic.
[[nodiscard]] bool prepare_main_candidate(MainCandidateState& state) noexcept;
} // namespace ps2::native
