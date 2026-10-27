#pragma once
#include "driving_assets.h"
#include "main_front_update.h"
#include "main_car_pose.h"

namespace ps2::native {
struct MainVehicleSetup {
    std::array<float,4> divisors{};
    float gravity=0;
};
// Inputs owned by the surrounding game flow and cabinet. No per-frame motion,
// force, collision result, selected tuning or internal history is supplied.
struct MainVehicleInputs {
    std::uint32_t frame=0,requested_gear=0,cabinet_screen=0;
    std::int32_t screen=0,operating_mode=0,variant_mode=0,shift_variant=0;
    std::uint8_t steering_sample=128,accelerator_sample=48,brake_sample=48;
    std::uint8_t flag_sample=0,cabinet_kind=0;
    bool manual_selection=false,surrounding_car_update=false;
};
struct MainVehicleState {
    // One owner for each persistent quantity. Stage adapters are transient.
    MainFrontState front{};
    std::uint16_t flags=0; // low half; slot packed_types owns the upper half
    float steering=0,steering_history=0;
    std::int16_t segment=0,progression=0;
    std::uint32_t gear=0;
    float rate=0,phase_step=0,phase=0;
    VehicleParameterRecord parameters{};
    MainBalancePower power{};
    MainBalanceSound sound{};
    float balance_history=0;
    std::uint32_t unloaded=0,parameter_index=0;
    std::uint32_t quiet_updates=0,motion_sound_active=0;
    std::array<std::uint32_t,3> diagnostic_marker{};
};
struct MainVehicleOutput {
    RenderPose pose{};
    std::int32_t selected_segment=0,progression=0;
    float speed_metric=0;
    std::array<MainBalanceCommand,72> commands{};
    unsigned command_count=0;
};
// Optional caller-owned observation. The clock must be side-effect-free with
// respect to game/asset state. Tick fields publish only after a successful
// update; rejection leaves the whole profile unchanged. A null clock performs
// no calls and publishes zero ticks. Total includes wrapper/adaptation work;
// front/balance/motion include appending their emitted command requests.
struct MainVehicleProfile {
    std::uint64_t (*clock)(void*) noexcept=nullptr;
    void* context=nullptr;
    std::uint64_t entry_ticks=0,contact_ticks=0,front_ticks=0;
    std::uint64_t balance_ticks=0,motion_ticks=0,total_ticks=0;
};
// Compose the recovered main callback, retaining the resulting state for the
// next call. The optional surrounding-car entry stage remains unsupported and
// rejects explicitly. Every constituent domain applies. State and publication
// commit together; failed stages cannot partially advance the persistent car.
[[nodiscard]] bool update_main_vehicle(const DrivingAssets& assets,
                                       const MainVehicleSetup& setup,
                                       const MainVehicleInputs& input,
                                       MainVehicleState& state,
                                       MainVehicleOutput& output,
                                       MainVehicleProfile* profile=nullptr) noexcept;
} // namespace ps2::native
