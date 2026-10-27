#pragma once
#include "main_balance_geometry.h"
#include "vehicle_parameter_selection.h"
#include <array>
#include <cstdint>
#include <span>

namespace ps2::native {
// Explicit immutable value assets. Names describe how a value is used; they
// do not establish physical units. No mapped pointers are retained.
struct MainBalanceAssets {
    std::array<VehicleParameterRecord,10> parameters{};
    std::array<float,2> limit_mix{}, balance_shape{};
    float lateral_deadzone=0;
    std::array<float,64> cabinet_center{}, cabinet_low{}, cabinet_high{};
    float cabinet_delta_gain=0, cabinet_force_gain=0;
    float ratio_scale=0;
    std::array<float,5> gear_ratios{};
    float inertia_base=0, inertia_ratio_scale=0, friction=0, drive_scale=0;
    float brake_scale=0, feedback_rate_scale=0, slip_boost=0;
    std::span<const float> power_curve;
    std::array<std::array<std::uint32_t,4>,10> sound_parameters{};
    std::array<std::uint32_t,8> occasional_commands{};
};

struct MainBalanceVehicle {
    std::uint32_t flags=0;
    AffinePoint3 motion{};
    float middle_orientation=0, steering=0;
    std::array<float,4> contact_weights{};
    std::uint32_t gear=0;
    float rate=0, phase_step=0, phase=0;
};

struct MainBalanceInputs {
    std::uint32_t frame=0, requested_gear=0, cabinet_screen=0;
    std::uint8_t steering_sample=128, accelerator_sample=48, brake_sample=48;
    std::uint8_t cabinet_kind=0;
    std::int32_t operating_mode=0, variant_mode=0, shift_variant=0;
    bool manual_selection=false;
    float speed_metric=0, reference_scale=0, angle_history=0;
    std::uint32_t contact_mask=0;
    float incoming_moment=0;
};

struct MainBalanceCabinet {
    float mix=0, steering_gain=0;
    std::int32_t output_limit=0, previous_quantized=0;
    float filtered=0, center_gain=0, delta_gain=0, history_gain=0;
    float current_steering=0, previous_steering=0;
    std::array<float,4> response_history{}, delta_history{};
    float prior_input=0;
    std::int32_t mode=0, countdown=0, impulse=0;
    std::uint32_t enabled=0;
    std::uint8_t output=0;
};

struct MainBalancePower {
    std::int32_t selection_delay=0, upshift_hold=0, downshift_hold=0;
    float correction=0, target_rate=0, ratio=0, previous_slip=0, factor=0;
    float slip=0, force=0;
    std::uint32_t last_pitch=0, last_level=0, pitch_command=0, level_command=0;
    std::uint32_t pitch_limit=0, level_limit=0, auxiliary_active=0;
    std::uint32_t auxiliary_start_command=0, auxiliary_level_command=0;
    std::int32_t occasional_count=0, occasional_mode=0;
    std::uint32_t random_state=0;
};

struct MainBalanceSound {
    std::uint32_t active=0, last_value=0, last_target=0;
    std::uint32_t continuation_bias=0, initial_bias=0;
    std::uint32_t initial_command=0, continuation_command=0, selected=0;
    std::int32_t unloaded_count=0;
};

struct MainBalanceUpdateState {
    MainBalanceVehicle vehicle{};
    MainBalanceGeometryState accumulated{};
    VehicleParameterRecord selected_parameters{};
    std::array<float,4> contact_sums{};
    float balance_history=0;
    std::uint32_t unloaded=0;
    MainBalanceCabinet cabinet{};
    MainBalancePower power{};
    MainBalanceSound sound{};
};

struct MainBalanceCommand {
    std::uint32_t code=0, parameter=0;
    bool has_parameter=false;
};
struct MainBalanceEffects {
    std::array<MainBalanceCommand,24> commands{};
    unsigned count=0;
};

// Whole supported balance/input operation. State and effects commit together;
// unsupported numeric/asset/control inputs leave both unchanged. No allocation
// or hardware calls occur. Full supported-domain documentation accompanies the
// composite original-program comparison, not a claim of a complete car tick.
[[nodiscard]] bool update_main_balance(const MainBalanceAssets& assets,
                                      const MainBalanceInputs& input,
                                      MainBalanceUpdateState& state,
                                      MainBalanceEffects& effects) noexcept;
} // namespace ps2::native
