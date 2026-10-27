#pragma once
#include "main_vehicle_update.h"
namespace ps2::native {
// Version1 wire order. Deliberately lists typed values instead of copying C++
// object storage: padding, ABI widths and host endianness never enter the file.
template<class Setup,class Inputs,class State,class Visit>
constexpr void visit_main_vehicle_checkpoint_values(Setup& setup,Inputs& input,State& state,Visit visit) {
    const auto list=[&](auto& values){for(auto& value:values)visit(value);};
    list(setup.divisors);visit(setup.gravity);
    visit(input.frame);visit(input.requested_gear);visit(input.cabinet_screen);visit(input.screen);
    visit(input.operating_mode);visit(input.variant_mode);visit(input.shift_variant);
    visit(input.steering_sample);visit(input.accelerator_sample);visit(input.brake_sample);visit(input.flag_sample);visit(input.cabinet_kind);
    visit(input.manual_selection);visit(input.surrounding_car_update);
    auto& front=state.front;auto& contact=front.contact;auto& vehicle=contact.vehicle;
    list(vehicle.position);list(vehicle.pending_position_delta);list(vehicle.motion);list(vehicle.orientation);list(vehicle.angular_motion);
    visit(vehicle.response_marker);list(vehicle.candidate_position);list(vehicle.candidate_orientation);
    list(contact.feedback);visit(contact.contact_mask);visit(contact.selected_scalar);
    for(auto& result:contact.contacts){list(result.direction);visit(result.gap);visit(result.status);}
    auto& slots=front.slots;visit(slots.packed_types);visit(slots.type_mask);list(slots.history);list(slots.published_middle);
    list(slots.gain);list(slots.metric);list(slots.lateral);
    auto& cabinet=front.cabinet;
    visit(cabinet.mix);visit(cabinet.steering_gain);visit(cabinet.output_limit);visit(cabinet.previous_quantized);
    visit(cabinet.filtered);visit(cabinet.center_gain);visit(cabinet.delta_gain);visit(cabinet.history_gain);
    visit(cabinet.current_steering);visit(cabinet.previous_steering);list(cabinet.response_history);list(cabinet.delta_history);
    visit(cabinet.prior_input);visit(cabinet.mode);visit(cabinet.countdown);visit(cabinet.impulse);visit(cabinet.enabled);visit(cabinet.output);
    visit(front.contact_event_active);visit(front.sound_active);visit(front.angle_history);visit(front.angle_change);list(front.contact_sums);
    visit(state.flags);visit(state.steering);visit(state.steering_history);visit(state.segment);visit(state.progression);
    visit(state.gear);visit(state.rate);visit(state.phase_step);visit(state.phase);list(state.parameters.words);
    auto& power=state.power;
    visit(power.selection_delay);visit(power.upshift_hold);visit(power.downshift_hold);visit(power.correction);visit(power.target_rate);
    visit(power.ratio);visit(power.previous_slip);visit(power.factor);visit(power.slip);visit(power.force);
    visit(power.last_pitch);visit(power.last_level);visit(power.pitch_command);visit(power.level_command);visit(power.pitch_limit);visit(power.level_limit);
    visit(power.auxiliary_active);visit(power.auxiliary_start_command);visit(power.auxiliary_level_command);
    visit(power.occasional_count);visit(power.occasional_mode);visit(power.random_state);
    auto& sound=state.sound;
    visit(sound.active);visit(sound.last_value);visit(sound.last_target);visit(sound.continuation_bias);visit(sound.initial_bias);
    visit(sound.initial_command);visit(sound.continuation_command);visit(sound.selected);visit(sound.unloaded_count);
    visit(state.balance_history);visit(state.unloaded);visit(state.parameter_index);visit(state.quiet_updates);visit(state.motion_sound_active);
    list(state.diagnostic_marker);
}
} // namespace ps2::native
