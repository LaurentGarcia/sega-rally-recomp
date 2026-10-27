#pragma once
// Offline reference adapter only. Never included by a native application.
#include "main_balance_update.h"
#include <bit>
#include <type_traits>
#include <vector>

namespace balance_update_test {
using namespace ps2::native;
template<class State,class Visit>
void state_words(State& s,std::uint32_t object,std::uint32_t first,std::uint32_t second,Visit visit){
    auto point=[&](std::uint32_t a,auto& p){visit(a,p.x);visit(a+4,p.y);visit(a+8,p.z);};
    auto list=[&](std::uint32_t a,auto& values){for(auto& value:values){visit(a,value);a+=4;}};
    visit(object+0x10,s.vehicle.flags);point(object+0x2c,s.vehicle.motion);
    visit(object+0x3c,s.vehicle.middle_orientation);visit(object+0x74,s.vehicle.steering);
    list(object+0x64,s.vehicle.contact_weights);visit(object+0x78,s.vehicle.gear);
    visit(object+0x7c,s.vehicle.rate);visit(object+0x80,s.vehicle.phase_step);visit(object+0x84,s.vehicle.phase);
    point(first,s.accumulated.first);point(second,s.accumulated.second);visit(0x214264,s.accumulated.published_index);
    list(0x2141c0,s.selected_parameters.words);list(0x21420c,s.contact_sums);
    visit(0x21425c,s.balance_history);visit(0x214260,s.unloaded);
    auto& c=s.cabinet;
    visit(0x214270,c.mix);visit(0x214274,c.output_limit);visit(0x214278,c.steering_gain);visit(0x21427c,c.previous_quantized);
    visit(0x214280,c.filtered);visit(0x214284,c.center_gain);visit(0x214288,c.delta_gain);visit(0x21428c,c.history_gain);
    visit(0x214290,c.current_steering);visit(0x214294,c.previous_steering);
    list(0x2142a0,c.response_history);list(0x2142b0,c.delta_history);
    visit(0x2142c4,c.prior_input);visit(0x2142c8,c.mode);visit(0x2142cc,c.countdown);visit(0x2142d0,c.impulse);
    visit(0x2142d4,c.enabled);visit(0x202049,c.output);
    auto& p=s.power;
    visit(0x2142e0,p.selection_delay);visit(0x2142e4,p.correction);visit(0x2142e8,p.target_rate);visit(0x2142ec,p.ratio);
    visit(0x2142f0,p.previous_slip);visit(0x2142f4,p.factor);visit(0x214254,p.slip);visit(0x214258,p.force);
    visit(0x2142fc,p.upshift_hold);visit(0x214300,p.downshift_hold);
    visit(0x214304,p.last_pitch);visit(0x214308,p.last_level);visit(0x21430c,p.pitch_command);visit(0x214310,p.level_command);
    visit(0x214314,p.pitch_limit);visit(0x214318,p.level_limit);visit(0x21431c,p.auxiliary_active);
    visit(0x214320,p.auxiliary_start_command);visit(0x214324,p.auxiliary_level_command);
    visit(0x21432c,p.occasional_count);visit(0x214330,p.occasional_mode);visit(0x2197b0,p.random_state);
    auto& sound=s.sound;
    visit(0x214230,sound.active);visit(0x214234,sound.last_value);visit(0x214238,sound.last_target);
    visit(0x21423c,sound.continuation_bias);visit(0x214240,sound.initial_bias);
    visit(0x214244,sound.initial_command);visit(0x214248,sound.continuation_command);
    visit(0x21424c,sound.selected);visit(0x214250,sound.unloaded_count);
}
template<class Read> MainBalanceInputs inputs(Read read,std::uint32_t moment){
    MainBalanceInputs in;
    const auto floating=[&](std::uint32_t a){return std::bit_cast<float>(read(a));};
    const auto signed_word=[&](std::uint32_t a){return std::bit_cast<std::int32_t>(read(a));};
    in.frame=read(0x202008);in.requested_gear=read(0x202044);in.cabinet_screen=read(0x202098);
    const auto pins=read(0x202050);in.steering_sample=pins&255;in.accelerator_sample=(pins>>8)&255;in.brake_sample=(pins>>16)&255;
    in.cabinet_kind=(read(0x202018)>>16)&255;
    in.operating_mode=signed_word(0x214120);in.variant_mode=signed_word(0x2020b4);in.shift_variant=signed_word(0x2139cc);
    in.manual_selection=read(0x2139d0)!=0;in.speed_metric=floating(0x20b0b4);in.reference_scale=floating(0x214124);
    in.angle_history=floating(0x214200);in.contact_mask=read(0x214208);in.incoming_moment=std::bit_cast<float>(moment);return in;
}
struct OwnedAssets {MainBalanceAssets values;std::array<float,104> curve;};
template<class Read>void assets(OwnedAssets& result,Read read,std::uint32_t curve){
    auto& a=result.values;
    const auto value=[&](std::uint32_t address){return std::bit_cast<float>(read(address));};
    for(unsigned i=0;i<10;++i){const auto pointer=read(0x5d67a0+4*i);for(unsigned j=0;j<13;++j)a.parameters[i].words[j]=read(pointer+4*j);}
    for(unsigned i=0;i<2;++i){a.limit_mix[i]=value(0x5d6680+4*i);a.balance_shape[i]=value(0x5d6688+4*i);}
    a.lateral_deadzone=value(0x5d6690);
    for(unsigned i=0;i<64;++i){a.cabinet_center[i]=value(0x5d8230+4*i);a.cabinet_low[i]=value(0x5d8330+4*i);a.cabinet_high[i]=value(0x5d8430+4*i);}
    a.cabinet_delta_gain=value(0x5d8530);a.cabinet_force_gain=value(0x5d8534);
    a.ratio_scale=value(0x5d9254);for(unsigned i=0;i<5;++i)a.gear_ratios[i]=value(0x5d9264+4*i);
    a.inertia_base=value(0x5d9300);a.inertia_ratio_scale=value(0x5d9304);a.friction=value(0x5d9308);
    a.drive_scale=value(0x5d930c);a.brake_scale=value(0x5d9310);a.feedback_rate_scale=value(0x5d9314);a.slip_boost=value(0x5d9318);
    for(unsigned i=0;i<104;++i)result.curve[i]=value(curve+4*i);a.power_curve=result.curve;
    for(unsigned i=0;i<10;++i)for(unsigned j=0;j<4;++j)a.sound_parameters[i][j]=read(0x5dcd10+16*i+4*j);
    for(unsigned i=0;i<8;++i)a.occasional_commands[i]=read(0x5d9e30+4*i);
}
template<class T>std::uint32_t bits(T value){
    if constexpr(sizeof(T)==1)return value;
    else return std::bit_cast<std::uint32_t>(value);
}
} // namespace balance_update_test
