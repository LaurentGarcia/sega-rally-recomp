#include "main_front_update.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using namespace ps2::native;
template<class T>auto bytes(const T& value){std::array<unsigned char,sizeof(T)> result;std::memcpy(result.data(),&value,sizeof value);return result;}
bool zero(AffinePoint3 p){return std::bit_cast<std::uint32_t>(p.x)==0&&std::bit_cast<std::uint32_t>(p.y)==0&&std::bit_cast<std::uint32_t>(p.z)==0;}
}
int main(){
    std::array<ContactNormalPair,8192> normals;normals.fill({.25f,1.5f});
    MainFrontInputs input;input.normal_table=normals;
    input.local_points={AffinePoint3{-1,0,1},AffinePoint3{1,0,1},AffinePoint3{-1,0,-1},AffinePoint3{1,0,-1}};
    MainFrontState state;
    for(auto& c:state.contact.contacts){c.status=128;c.gap=0;c.direction={0,1,0};}
    state.angle_history=.5f;state.angle_change=.25f;state.sound_active=1;
    MainFrontOutput output;MainBalanceEffects effects;
    if(!update_main_front(input,state,output,effects)||!zero(output.force)||!zero(output.moment)||
       output.speed_metric!=0||output.incoming_moment!=0||effects.count||state.sound_active||
       state.angle_history!=0||state.angle_change!=0||state.contact.vehicle.response_marker||
       state.slots.type_mask!=1)return 1;
    unsigned checks=12;
    // Failure at input validation, table validation, and after four responses
    // have been calculated must preserve all public state/output/request bytes.
    const auto original_state=bytes(state);
    const auto original_output=bytes(output);
    const auto original_effects=bytes(effects);
    for(unsigned failure=0;failure<4;++failure){auto bad=input;
        if(failure==0)bad.steering=std::numeric_limits<float>::infinity();
        if(failure==1)bad.normal_table=std::span(normals).first(8191);
        if(failure==2)bad.local_points[3].z=std::numeric_limits<float>::quiet_NaN();
        if(failure==3){bad.scene_metric_scale=true;bad.control=0;}
        if(update_main_front(bad,state,output,effects)||bytes(state)!=original_state||bytes(output)!=original_output||bytes(effects)!=original_effects)return 1;
        checks+=4;
    }
    // Exactly the original event transition for material-bit4: one request on
    // entry, then no duplicate request while that material remains active.
    for(auto& c:state.contact.contacts)c.status=4;
    input.slot_parameters={1,1,1,1,1};state.sound_active=0;
    if(!update_main_front(input,state,output,effects)||effects.count!=1||effects.commands[0].code!=0x7b||
       effects.commands[0].has_parameter||state.sound_active!=1||state.contact.vehicle.response_marker!=15)return 1;
    checks+=6;
    if(!update_main_front(input,state,output,effects)||effects.count)return 1;
    checks+=2;
    std::printf("MAIN_FRONT_UPDATE PASS checks=%u atomic_rejections=4 event_transition=1\n",checks);return 0;
}
