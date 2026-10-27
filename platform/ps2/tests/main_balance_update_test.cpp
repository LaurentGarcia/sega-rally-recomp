#include "main_balance_update.h"
#include <bit>
#include <cstdio>
#include <cstring>

namespace {
using namespace ps2::native;
struct Fixture {
    std::array<float,104> curve;
    MainBalanceAssets assets;
    MainBalanceInputs input;
    MainBalanceUpdateState state;
    Fixture(){
        curve.fill(10);assets.power_curve=curve;
        for(auto& p:assets.parameters){p.words[0]=p.words[1]=p.words[2]=0x3f000000;p.words[9]=p.words[10]=0x3dcccccd;}
        assets.limit_mix={0.5f,0.5f};assets.balance_shape={0.125f,0.125f};
        for(unsigned i=0;i<64;++i){assets.cabinet_center[i]=float(i);assets.cabinet_low[i]=float(i)/2;assets.cabinet_high[i]=float(i);}
        assets.cabinet_delta_gain=0.5f;assets.cabinet_force_gain=0.125f;
        assets.ratio_scale=1;assets.gear_ratios.fill(1);assets.inertia_base=1;assets.inertia_ratio_scale=1;
        assets.friction=1;assets.drive_scale=1;assets.brake_scale=1;assets.feedback_rate_scale=0.125f;assets.slip_boost=0.125f;
        for(auto& row:assets.sound_parameters)row={0xb2,0xb3,1,2};assets.occasional_commands.fill(0x75);
        input.frame=8;input.operating_mode=1;input.reference_scale=1;input.speed_metric=0.125f;
        input.accelerator_sample=191;input.steering_sample=128;
        state.vehicle.motion={0,0,1};state.vehicle.contact_weights={0.25f,0.25f,0.25f,0.25f};
        state.vehicle.gear=1;state.vehicle.rate=2000;state.power.target_rate=2000;
        state.power.pitch_limit=127;state.power.level_limit=32;state.power.pitch_command=0xad;state.power.level_command=0xae;
        state.power.auxiliary_start_command=0xb9;state.power.auxiliary_level_command=0xba;
        state.power.random_state=0x12345678;state.contact_sums={0.125f,1,-0.125f,1};
        state.cabinet.output_limit=31;state.cabinet.center_gain=1;state.cabinet.delta_gain=1;state.cabinet.history_gain=0.125f;
    }
};
unsigned checks=0;
bool require(bool condition){++checks;return condition;}
template<class Change>bool rejection(Change change){
    Fixture f;change(f);MainBalanceEffects output;output.count=17;output.commands[3]={0x1234,0x5678,true};
    std::array<unsigned char,sizeof(f.state)> before;std::array<unsigned char,sizeof(output)> prior;
    std::memcpy(before.data(),&f.state,sizeof(f.state));std::memcpy(prior.data(),&output,sizeof(output));
    return require(!update_main_balance(f.assets,f.input,f.state,output))&&
           require(std::memcmp(before.data(),&f.state,sizeof(f.state))==0)&&require(std::memcmp(prior.data(),&output,sizeof(output))==0);
}
}
int main(){
    for(unsigned i=0;i<192;++i){
        Fixture f;f.input.frame=i;f.input.operating_mode=i%3;f.input.variant_mode=(i>>2)&1;
        f.input.manual_selection=(i>>3)&1;f.input.requested_gear=(i/3)%5;f.state.vehicle.gear=(i/7)%5;
        f.state.power.selection_delay=31;f.state.power.upshift_hold=(i>>4)&1?6:0;f.state.power.downshift_hold=(i>>5)&1?-8:0;
        f.state.cabinet.mode=i%5;f.state.cabinet.countdown=i%3;f.state.cabinet.impulse=int(i%65)-32;
        f.state.power.occasional_mode=i%3;f.state.power.occasional_count=61;f.input.contact_mask=1u<<(i%10);
        MainBalanceEffects output;
        if(!require(update_main_balance(f.assets,f.input,f.state,output))||!require(output.count<=output.commands.size())||
           !require(f.state.accumulated.published_index==i%10))return 1;
    }
    if(!rejection([](auto& f){f.assets.power_curve=f.assets.power_curve.first(81);})||
       !rejection([](auto& f){f.input.reference_scale=0;})||
       !rejection([](auto& f){f.input.requested_gear=5;})||
       !rejection([](auto& f){f.state.vehicle.middle_orientation=std::bit_cast<float>(0x7fc12345u);})||
       !rejection([](auto& f){f.assets.gear_ratios[4]=0;})||
       !rejection([](auto& f){f.state.power.pitch_limit=0;})||
       !rejection([](auto& f){f.assets.parameters[0].words[9]=0x7f800000u;})||
       !rejection([](auto& f){f.state.cabinet.prior_input=std::bit_cast<float>(0x7f800000u);}))return 1;
    // State owns the inputs used here; the explicit input value is copied
    // before the final commit, so aliasing an effect count with state is not
    // an advertised API. This tests a real late rejection after power/events.
    if(!rejection([](auto& f){f.state.contact_sums[0]=16;f.state.cabinet.mode=0;}))return 1;
    std::printf("MAIN_BALANCE_UPDATE PASS branches=192 atomic_rejections=9 checks=%u\n",checks);return 0;
}
