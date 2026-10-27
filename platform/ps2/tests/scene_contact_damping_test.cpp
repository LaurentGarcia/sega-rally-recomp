// Original numerical controls and atomic motion-integration checks, no assets.
#include "scene_contact_damping.h"
#include "main_motion_update.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using namespace ps2::native;
unsigned checks=0;
void require(bool condition){++checks;if(!condition){std::fprintf(stderr,"check=%u\n",checks);throw std::runtime_error("scene damping check failed");}}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
float value(std::uint32_t word){return std::bit_cast<float>(word);}
float mul(float a,float b){volatile float x=a*b;return x;}
float add(float a,float b){volatile float x=a+b;return x;}
float div(float a,float b){volatile float x=a/b;return x;}
float expected(float base,float direction){
    // Independent ordinary-float evaluation of the recovered metric at unit
    // X separation. Its established correction is not sqrt(1)==1.
    const auto seed=add(value(0x3ed5a9a8),mul(.5f,value(0x3f1714ba)));
    const auto halfway=mul(add(1,seed),.5f);
    const auto corrected=mul(add(div(.5f,halfway),halfway),.5f);
    const auto metric=mul(mul(corrected,value(0x3f3504f4)),2);
    const auto quotient=div(direction,mul(metric,metric));
    volatile double product=double(quotient)*0.005;
    volatile double sum=double(base)+product;
    return sum>1?1.0f:float(sum);
}
}
int main(){try{
    for(float base:{-0.0f,0.0f,-2.0f,.5f,1.0f,2.0f}){
        for(float direction:{-1.0f,-0.0f,0.0f}){
            float output=value(0x7fc12345);
            require(correct_scene_contact_damping({{1,0,0},{0,0,0},{direction,0,0}},base,output));
            require(bits(output)==bits(base));
        }
        for(float direction:{value(1),.25f,1.0f,1048576.0f}){
            float output=value(0x7fc12345);
            require(correct_scene_contact_damping({{1,0,0},{0,0,0},{direction,0,0}},base,output));
            require(bits(output)==bits(expected(base,direction)));
        }
    }
    // The original wire pairing is not generic Euclidean distance: equal
    // nonzero Y/Z separation gives a zero metric, and is explicitly rejected.
    for(const SceneContactGeometry geometry:{
            SceneContactGeometry{{0,1,1},{0,0,0},{0,1,0}},
            SceneContactGeometry{{value(1),0,0},{0,0,0},{1,0,0}},
            SceneContactGeometry{{value(0x49800001),0,0},{0,0,0},{0,0,0}},
            SceneContactGeometry{{1,0,0},{0,0,0},{value(0x7f800000),0,0}},
            SceneContactGeometry{{1,0,0},{value(0x7fc12345),0,0},{0,0,0}}}){
        float output=value(0x7fc54321);
        require(!correct_scene_contact_damping(geometry,.5f,output));
        require(bits(output)==0x7fc54321);
    }
    float output=17;
    require(!correct_scene_contact_damping({{1,0,0},{0,0,0},{1,0,0}},value(0x7fc12345),output));
    require(output==17);
    // Cancellation at the ordered dot-product boundary selects the copy arm.
    require(correct_scene_contact_damping({{1,1,1},{0,0,0},{1,-1,0}},-0.0f,output));
    require(bits(output)==0x80000000);
    // Used scalar storage may be the output; all reads precede publication.
    output=.5f;require(correct_scene_contact_damping({{1,0,0},{0,0,0},{1,0,0}},output,output));
    require(bits(output)==bits(expected(.5f,1)));

    MainMotionInput input{};input.mode=1;input.frame=1;
    input.divisors={1,1,1,1};input.contact_damping=.5f;
    input.scene_correction_active=true;input.scene_geometry={{1,0,0},{0,0,0},{1,0,0}};
    MainMotionState state{};state.active_contacts=15;state.vehicle.motion={.25f,0,.125f};
    MainBalanceEffects effects{};effects.count=2;effects.commands[0]={99,31,true};
    require(update_main_motion(input,state,effects));
    require(bits(state.vehicle.position[0])==bits(.25f)&&bits(state.vehicle.position[2])==bits(.125f));
    require(bits(state.vehicle.motion[0])==bits(mul(expected(.5f,1),.25f)));
    require(bits(state.vehicle.motion[2])==bits(mul(expected(.5f,1),.125f)));
    require(effects.count==0);
    input.scene_geometry={{0,1,1},{0,0,0},{0,1,0}};
    const auto before=state;const auto before_effects=effects;
    require(!update_main_motion(input,state,effects));
    require(std::memcmp(&state,&before,sizeof state)==0);
    require(std::memcmp(&effects,&before_effects,sizeof effects)==0);
    // Inactive scene data remains unused, even if it is not a valid geometry.
    input.scene_correction_active=false;
    input.scene_geometry.published_point.x=value(0x7fc12345);
    const auto previous_x=state.vehicle.motion[0];
    require(update_main_motion(input,state,effects));
    require(bits(state.vehicle.motion[0])==bits(mul(.5f,previous_x)));
    std::printf("SCENE_CONTACT_DAMPING PASS checks=%u active_motion=1 atomic_rejection=1\n",checks);
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}return 0;}
