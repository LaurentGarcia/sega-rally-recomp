#pragma once
// Offline boundary adapter. Original addresses are never native game inputs.
#include "main_motion_update.h"
#include "driving_assets.h"
#include <bit>

namespace motion_update_test {
using namespace ps2::native;
template<class State,class Visit>
void state_words(State& s,std::uint32_t object,std::uint32_t frame,Visit visit) {
    const auto list=[&](std::uint32_t address,auto& values){for(auto& value:values){visit(address,value);address+=4;}};
    auto& v=s.vehicle;
    list(object+0x14,v.position);list(object+0x20,v.pending_position_delta);
    list(object+0x2c,v.motion);list(object+0x38,v.orientation);list(object+0x44,v.angular_motion);
    list(frame+0x130,v.candidate_position);list(frame+0x140,v.candidate_orientation);
    visit(0x213974,v.response_marker);
    auto& c=s.cabinet;
    visit(0x214270,c.mix);visit(0x214274,c.output_limit);visit(0x214278,c.steering_gain);visit(0x21427c,c.previous_quantized);
    visit(0x214280,c.filtered);visit(0x214284,c.center_gain);visit(0x214288,c.delta_gain);visit(0x21428c,c.history_gain);
    visit(0x214290,c.current_steering);visit(0x214294,c.previous_steering);
    list(0x2142a0,c.response_history);list(0x2142b0,c.delta_history);
    visit(0x2142c4,c.prior_input);visit(0x2142c8,c.mode);visit(0x2142cc,c.countdown);visit(0x2142d0,c.impulse);
    visit(0x2142d4,c.enabled);visit(0x202049,c.output);
    list(0x213964,s.feedback);visit(0x213974,s.active_contacts);visit(0x214208,s.contact_types);
    visit(0x213870,s.quiet_updates);visit(0x21396c,s.contact_event_active);visit(0x2138c4,s.sound_active);
    visit(0x2197b0,s.random_state);list(0x20b1a4,s.diagnostic_marker);
}
template<class Read>
MainMotionInput inputs(Read read,std::uint32_t object,std::uint32_t frame,const DrivingMotionAssets& assets) {
    const auto f=[&](std::uint32_t a){return std::bit_cast<float>(read(a));};
    const auto point=[&](std::uint32_t a){return AffinePoint3{f(a),f(a+4),f(a+8)};};
    MainMotionInput input;
    input.force=point(frame+0x120);input.moment=point(frame+0x150);
    input.forward=point(frame+0xac);input.reference_direction=point(0x214360);
    for(unsigned i=0;i<4;++i)input.divisors[i]=f(object+0x50+4*i);
    input.selected_scalar=f(frame+0x1a0);input.speed_metric=f(0x20b0b4);input.angle_change=f(0x214204);
    input.gravity=f(0x214124);input.orientation_shape=assets.scalar;
    input.middle_response_gain=f(0x2141cc);input.middle_motion_gain=f(0x2141d0);
    input.contact_damping=f(0x2141ec);input.selection_damping=f(0x2141f0);
    input.mode=read(0x214120);input.descriptor=read(0x214354);input.frame=read(0x202008);
    input.scene_correction_active=std::bit_cast<std::int32_t>(read(0x20a560))>0&&read(0x2139f8)!=0;
    if(input.scene_correction_active){const auto published=read(0x2139f8),reference=read(0x213980);
        input.scene_geometry={point(published),point(reference),point(published+12)};}
    input.rough_commands=assets.commands[0];input.contact_commands=assets.commands[1];
    return input;
}
} // namespace motion_update_test
