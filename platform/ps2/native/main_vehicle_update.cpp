#include "main_vehicle_update.h"
#include "main_update_entry.h"
#include "main_motion_update.h"
#include <bit>

namespace ps2::native {
namespace {
std::uint32_t flags(const MainVehicleState& state) noexcept {
    return state.flags|(std::uint32_t(state.front.slots.packed_types)<<16);
}
bool append(MainVehicleOutput& output,const MainBalanceEffects& source) noexcept {
    if(source.count>source.commands.size()||source.count>output.commands.size()-output.command_count)return false;
    for(unsigned i=0;i<source.count;++i)output.commands[output.command_count++]=source.commands[i];
    return true;
}
}
bool update_main_vehicle(const DrivingAssets& assets,const MainVehicleSetup& setup,
                         const MainVehicleInputs& input,MainVehicleState& output_state,
                         MainVehicleOutput& output) noexcept {
    if(!assets.loaded())return false;
    MainVehicleState state=output_state;
    MainVehicleOutput published{};
    // The surrounding original course draw publishes this immutable row before
    // the car callback. Its index is the prior segment, before entry reselects.
    const auto guidance_index=state.segment<0?0u:unsigned(state.segment);
    if(guidance_index>=assets.course_directions().size())return false;
    const auto guidance=assets.course_directions()[guidance_index];
    const auto tuning=[&](unsigned word){return std::bit_cast<float>(state.parameters.words[word]);};
    auto& car=state.front.contact.vehicle;
    MainEntryInput entry_input{assets.selection_vertices(),assets.selection_segments(),
        {car.position[0],car.position[2]},input.steering_sample,input.flag_sample,input.screen,input.surrounding_car_update};
    MainEntryState entry{flags(state),state.steering_history,state.steering,state.segment,state.progression};
    if(!begin_main_update(entry_input,entry))return false;
    state.flags=std::uint16_t(entry.flags);state.steering=entry.steering;state.steering_history=entry.steering_history;
    state.segment=entry.segment;state.progression=entry.progression;
    published.selected_segment=entry.published_segment;published.progression=entry.published_progression;

    MainContactStepInput contact_input;
    contact_input.course=assets.course();contact_input.normal_table=assets.normal_table();
    contact_input.local_points=*assets.local_points();contact_input.segment=state.segment;
    contact_input.response_parameter=setup.divisors[0];contact_input.gain_parameter=setup.divisors[1];
    contact_input.scalar_divisor=setup.divisors[2];contact_input.feedback_enabled=state.front.cabinet.mode==0;
    if(!resolve_main_contact_step(contact_input,state.front.contact))return false;
    MainFrontInputs front_input;
    front_input.course=assets.course();front_input.normal_table=assets.normal_table();
    front_input.local_points=assets.motion_assets()->local_points;front_input.segment=state.segment;
    for(unsigned i=0;i<5;++i)front_input.slot_parameters[i]=tuning(4+i);
    front_input.steering=state.steering;front_input.object_parameter=setup.divisors[0];front_input.control=setup.gravity;
    front_input.longitudinal_metric=input.variant_mode!=0;front_input.scene_metric_scale=input.surrounding_car_update;
    MainFrontOutput front_output;MainBalanceEffects effects;
    if(!update_main_front(front_input,state.front,front_output,effects)||!append(published,effects))return false;

    MainBalanceUpdateState balance;
    balance.vehicle={flags(state),{car.motion[0],car.motion[1],car.motion[2]},car.orientation[1],state.steering,
        state.front.slots.history,state.gear,state.rate,state.phase_step,state.phase};
    balance.accumulated={front_output.force,front_output.moment,state.parameter_index};
    balance.selected_parameters=state.parameters;balance.contact_sums=state.front.contact_sums;
    balance.balance_history=state.balance_history;balance.unloaded=state.unloaded;
    balance.cabinet=state.front.cabinet;balance.power=state.power;balance.sound=state.sound;
    MainBalanceInputs balance_input;
    balance_input.frame=input.frame;balance_input.requested_gear=input.requested_gear;balance_input.cabinet_screen=input.cabinet_screen;
    balance_input.steering_sample=input.steering_sample;balance_input.accelerator_sample=input.accelerator_sample;
    balance_input.brake_sample=input.brake_sample;balance_input.cabinet_kind=input.cabinet_kind;
    balance_input.operating_mode=input.operating_mode;balance_input.variant_mode=input.variant_mode;
    balance_input.shift_variant=input.shift_variant;balance_input.manual_selection=input.manual_selection;
    balance_input.speed_metric=front_output.speed_metric;balance_input.reference_scale=setup.gravity;
    balance_input.angle_history=state.front.angle_history;balance_input.contact_mask=state.front.slots.type_mask;
    balance_input.incoming_moment=front_output.incoming_moment;
    if(!update_main_balance(*assets.balance_assets(input.manual_selection?1:0),balance_input,balance,effects)||!append(published,effects))return false;
    state.flags=std::uint16_t(balance.vehicle.flags);state.front.slots.packed_types=std::uint16_t(balance.vehicle.flags>>16);
    car.motion={balance.vehicle.motion.x,balance.vehicle.motion.y,balance.vehicle.motion.z};
    car.orientation[1]=balance.vehicle.middle_orientation;state.steering=balance.vehicle.steering;
    state.front.slots.history=balance.vehicle.contact_weights;state.gear=balance.vehicle.gear;
    state.rate=balance.vehicle.rate;state.phase_step=balance.vehicle.phase_step;state.phase=balance.vehicle.phase;
    state.parameters=balance.selected_parameters;state.front.contact_sums=balance.contact_sums;
    state.balance_history=balance.balance_history;state.unloaded=balance.unloaded;state.parameter_index=balance.accumulated.published_index;
    state.front.cabinet=balance.cabinet;state.power=balance.power;state.sound=balance.sound;

    MainMotionInput motion_input;
    motion_input.force=balance.accumulated.first;motion_input.moment=balance.accumulated.second;
    motion_input.forward=front_output.forward;motion_input.reference_direction={guidance.x,0,guidance.z};
    motion_input.divisors=setup.divisors;motion_input.selected_scalar=state.front.contact.selected_scalar;
    motion_input.speed_metric=front_output.speed_metric;motion_input.angle_change=state.front.angle_change;
    motion_input.gravity=setup.gravity;motion_input.orientation_shape=assets.motion_assets()->scalar;
    motion_input.middle_response_gain=tuning(3);motion_input.middle_motion_gain=tuning(4);
    motion_input.contact_damping=tuning(11);motion_input.selection_damping=tuning(12);
    motion_input.mode=std::bit_cast<std::uint32_t>(input.operating_mode);motion_input.descriptor=assets.selector();motion_input.frame=input.frame;
    motion_input.rough_commands=assets.motion_assets()->commands[0];motion_input.contact_commands=assets.motion_assets()->commands[1];
    MainMotionState motion;
    motion.vehicle=car;motion.cabinet=state.front.cabinet;motion.feedback=state.front.contact.feedback;
    motion.active_contacts=car.response_marker;motion.contact_types=state.front.slots.type_mask;
    motion.quiet_updates=state.quiet_updates;motion.contact_event_active=state.front.contact_event_active;
    motion.sound_active=state.motion_sound_active;motion.random_state=state.power.random_state;motion.diagnostic_marker=state.diagnostic_marker;
    if(!update_main_motion(motion_input,motion,effects)||!append(published,effects))return false;
    car=motion.vehicle;state.front.cabinet=motion.cabinet;state.front.contact.feedback=motion.feedback;
    car.response_marker=motion.active_contacts;state.front.slots.type_mask=motion.contact_types;
    state.quiet_updates=motion.quiet_updates;state.front.contact_event_active=motion.contact_event_active;
    state.motion_sound_active=motion.sound_active;state.power.random_state=motion.random_state;state.diagnostic_marker=motion.diagnostic_marker;
    MainCarPoseState pose;
    for(unsigned i=0;i<3;++i){pose.position[i]=std::bit_cast<std::uint32_t>(car.position[i]);
        pose.motion[i]=std::bit_cast<std::uint32_t>(car.motion[i]);pose.orientation[i]=std::bit_cast<std::uint32_t>(car.orientation[i]);}
    pose.attributes[0]=std::bit_cast<std::uint32_t>(state.steering);pose.attributes[1]=std::bit_cast<std::uint32_t>(state.phase_step);
    pose.attributes[2]=std::bit_cast<std::uint32_t>(state.phase);
    MainCarExternalState external;
    for(unsigned i=0;i<4;++i){pose.attributes[3+i]=std::bit_cast<std::uint32_t>(state.front.slots.history[i]);
        external[i]=std::bit_cast<std::uint32_t>(state.front.slots.published_middle[i]);}
    pose.flags=flags(state);pose.index=std::uint32_t(std::bit_cast<std::uint16_t>(state.segment))|
        (std::uint32_t(std::bit_cast<std::uint16_t>(state.progression))<<16);
    published.pose=make_main_car_pose(pose,external);published.speed_metric=front_output.speed_metric;
    output_state=state;output=published;return true;
}
} // namespace ps2::native
