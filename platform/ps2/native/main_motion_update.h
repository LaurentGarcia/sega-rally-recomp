#pragma once
#include "main_candidate.h"
#include "main_balance_update.h"
#include "scene_contact_damping.h"
#include <array>

namespace ps2::native {
struct MainMotionInput {
    AffinePoint3 force{}, moment{}, forward{}, reference_direction{};
    std::array<float,4> divisors{};
    float selected_scalar=0, speed_metric=0, angle_change=0, gravity=0;
    float orientation_shape=0, middle_response_gain=0, middle_motion_gain=0;
    float contact_damping=0, selection_damping=0;
    std::uint32_t mode=0, descriptor=0, frame=0;
    // Caller resolves the surrounding scene's ordinary geometry into values;
    // no original pointers survive this boundary. Used only on that branch.
    bool scene_correction_active=false;
    SceneContactGeometry scene_geometry{};
    std::array<std::uint32_t,32> rough_commands{}, contact_commands{};
};
struct MainMotionState {
    MainCandidateState vehicle{};
    MainBalanceCabinet cabinet{};
    std::array<std::int32_t,2> feedback{};
    std::uint32_t active_contacts=0, contact_types=0;
    std::uint32_t quiet_updates=0, contact_event_active=0, sound_active=0;
    std::uint32_t random_state=0;
    std::array<std::uint32_t,3> diagnostic_marker{};
};
// Post-balance motion, counter/event and final orientation region. This is an
// integration stage, not a complete car tick: its force/moment must be computed
// by the preceding native stages. No captured motion is a runtime input.
// Finite scalar inputs/used state have magnitude <=2^20; divisors have magnitude
// >=2^-20. Angle folding and the recovered metric retain their own domains.
// State and ordered command requests commit together; rejection preserves both.
[[nodiscard]] bool update_main_motion(const MainMotionInput& input,
                                     MainMotionState& state,
                                     MainBalanceEffects& effects) noexcept;
} // namespace ps2::native
