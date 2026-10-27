#pragma once
// Offline mapping of canonical typed values to observed ordinary memory.
// Native applications must not import this evidence adapter.
#include "main_vehicle_update.h"
#include "main_front_update_adapter.h"
#include <bit>
namespace vehicle_update_test {
using namespace ps2::native;
template<class State,class Visit>
void state_words(State& s,std::uint32_t object,std::uint32_t fp,Visit visit){
    front_update_test::state_words(s.front,object,fp,visit);
    const auto list=[&](std::uint32_t a,auto& values){for(auto& v:values){visit(a,v);a+=4;}};
    visit(fp+0x1a0,s.front.contact.selected_scalar);
    visit(object+0x10,s.flags);visit(object+0x74,s.steering);visit(0x2142c0,s.steering_history);
    visit(object+0x88,s.segment);visit(object+0x8a,s.progression);visit(object+0x78,s.gear);
    visit(object+0x7c,s.rate);visit(object+0x80,s.phase_step);visit(object+0x84,s.phase);
    list(0x2141c0,s.parameters.words);
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
    visit(0x21425c,s.balance_history);visit(0x214260,s.unloaded);visit(0x214264,s.parameter_index);
    visit(0x213870,s.quiet_updates);visit(0x2138c4,s.motion_sound_active);list(0x20b1a4,s.diagnostic_marker);
}
template<class Read>MainVehicleSetup setup(Read read,std::uint32_t object){
    MainVehicleSetup s;for(unsigned i=0;i<4;++i)s.divisors[i]=std::bit_cast<float>(read(object+0x50+4*i));
    s.gravity=std::bit_cast<float>(read(0x214124));
    return s;
}
inline MainVehicleInputs inputs(const std::array<std::uint32_t,12>& w){
    MainVehicleInputs in;in.frame=w[0];in.requested_gear=w[1];in.cabinet_screen=w[2];
    in.screen=std::bit_cast<std::int32_t>(w[3]);in.operating_mode=std::bit_cast<std::int32_t>(w[4]);
    in.variant_mode=std::bit_cast<std::int32_t>(w[5]);in.shift_variant=std::bit_cast<std::int32_t>(w[6]);
    in.steering_sample=w[7]&255;in.accelerator_sample=(w[7]>>8)&255;in.brake_sample=(w[7]>>16)&255;
    in.flag_sample=w[8]&255;in.cabinet_kind=(w[9]>>16)&255;in.manual_selection=w[10]!=0;
    in.surrounding_car_update=std::bit_cast<std::int32_t>(w[11])>0;return in;
}
} // namespace vehicle_update_test
