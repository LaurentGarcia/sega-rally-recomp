#pragma once
#include "main_contact_step.h"
#include "main_contact_slot.h"
#include "main_balance_update.h"

namespace ps2::native {
struct MainFrontInputs {
    std::span<const ContactSegment> course;
    std::span<const ContactNormalPair> normal_table;
    std::array<AffinePoint3,4> local_points{};
    std::array<float,5> slot_parameters{};
    std::int16_t segment=0;
    float steering=0, object_parameter=1, control=1;
    bool longitudinal_metric=false, scene_metric_scale=false;
};
struct MainFrontState {
    MainContactStepState contact{};
    MainContactSlotState slots{};
    MainBalanceCabinet cabinet{};
    std::uint32_t contact_event_active=0, sound_active=0;
    float angle_history=0, angle_change=0;
    std::array<float,4> contact_sums{};
};
struct MainFrontOutput {
    AffinePoint3 force{}, moment{}, forward{};
    float speed_metric=0, incoming_moment=0;
};
// Complete supported pre-balance stage: absolute contact transforms, requested
// re-queries, four slot responses, angle history and force/moment aggregation.
// Existing typed component domains apply. All used input coordinates/scalars
// must be finite with magnitude <=2^20; orientation/steering magnitudes <=8.
// Table-normalized motion preserves the full binary32 squared-length encoding,
// including zero and subnormals, in its coefficient selection.
// State, result and ordered requests commit together, or remain unchanged.
// No captured intermediate, mapped memory, processor or hardware state is used.
[[nodiscard]] bool update_main_front(const MainFrontInputs& input,
                                    MainFrontState& state,
                                    MainFrontOutput& output,
                                    MainBalanceEffects& effects) noexcept;
} // namespace ps2::native
