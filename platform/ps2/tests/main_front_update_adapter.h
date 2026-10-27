#pragma once
// Offline evidence adapter. No original addresses enter the native API.
#include "main_front_update.h"
#include "driving_assets.h"
#include <bit>
namespace front_update_test {
using namespace ps2::native;
template<class State,class Visit>
void state_words(State& s,std::uint32_t object,std::uint32_t fp,Visit visit){
    const auto list=[&](std::uint32_t a,auto& values){for(auto& v:values){visit(a,v);a+=4;}};
    auto& v=s.contact.vehicle;
    list(object+0x14,v.position);list(object+0x20,v.pending_position_delta);list(object+0x2c,v.motion);
    list(object+0x38,v.orientation);list(object+0x44,v.angular_motion);
    list(fp+0x130,v.candidate_position);list(fp+0x140,v.candidate_orientation);visit(0x213974,v.response_marker);
    list(0x213964,s.contact.feedback);visit(0x213960,s.contact.contact_mask);
    for(unsigned i=0;i<4;++i){auto& r=s.contact.contacts[i];const auto a=0x2138d0+36*i;
        list(a+12,r.direction);visit(a+24,r.gap);visit(a+28,r.status);}
    visit(object+0x12,s.slots.packed_types);visit(0x214208,s.slots.type_mask);
    list(object+0x64,s.slots.history);list(0x2138a0,s.slots.published_middle);list(0x2138b0,s.slots.gain);
    list(0x213890,s.slots.metric);list(0x213880,s.slots.lateral);
    auto& c=s.cabinet;
    visit(0x214270,c.mix);visit(0x214274,c.output_limit);visit(0x214278,c.steering_gain);visit(0x21427c,c.previous_quantized);
    visit(0x214280,c.filtered);visit(0x214284,c.center_gain);visit(0x214288,c.delta_gain);visit(0x21428c,c.history_gain);
    visit(0x214290,c.current_steering);visit(0x214294,c.previous_steering);
    list(0x2142a0,c.response_history);list(0x2142b0,c.delta_history);
    visit(0x2142c4,c.prior_input);visit(0x2142c8,c.mode);visit(0x2142cc,c.countdown);visit(0x2142d0,c.impulse);
    visit(0x2142d4,c.enabled);visit(0x202049,c.output);
    visit(0x21396c,s.contact_event_active);visit(0x2138c0,s.sound_active);
    visit(0x214200,s.angle_history);visit(0x214204,s.angle_change);list(0x21420c,s.contact_sums);
}
template<class Result,class Visit>
void output_words(Result& r,std::uint32_t fp,Visit visit){
    auto point=[&](std::uint32_t a,auto& p){visit(a,p.x);visit(a+4,p.y);visit(a+8,p.z);};
    point(fp+0x120,r.force);point(fp+0x150,r.moment);point(fp+0xac,r.forward);visit(0x20b0b4,r.speed_metric);
}
template<class Read>
MainFrontInputs inputs(Read read,std::uint32_t object,const DrivingAssets& assets){
    const auto f=[&](std::uint32_t a){return std::bit_cast<float>(read(a));};
    MainFrontInputs in;
    in.course=assets.course();in.normal_table=assets.normal_table();in.local_points=assets.motion_assets()->local_points;
    for(unsigned i=0;i<5;++i)in.slot_parameters[i]=f(0x2141d0+4*i);
    if(read(0x213960))in.segment=std::bit_cast<std::int16_t>(std::uint16_t(read(0x2140c8)));
    in.steering=f(object+0x74);in.object_parameter=f(object+0x50);in.control=f(0x214124);
    in.longitudinal_metric=read(0x2020b4)!=0;in.scene_metric_scale=std::bit_cast<std::int32_t>(read(0x20a560))>0;
    return in;
}
} // namespace front_update_test
