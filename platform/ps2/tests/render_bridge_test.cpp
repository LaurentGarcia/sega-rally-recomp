#include "car_render_bridge.h"
#include "driving_render_view.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
using namespace ps2::native;
unsigned checks=0;
void require(bool okay){++checks;if(!okay)throw checks;}
float value(std::uint32_t v){return std::bit_cast<float>(v);}
template<class T> bool same(const T&a,const T&b){return std::memcmp(&a,&b,sizeof a)==0;}
AffineTransform3 identity(){AffineTransform3 m;m.columns={AffinePoint3{1,0,0},AffinePoint3{0,1,0},AffinePoint3{0,0,1}};return m;}
using BridgeFunction=bool (*)(const RenderPose&,const CarRenderGeometry&,
                             const AffineTransform3&,CarRenderBridge&) noexcept;
std::uint64_t rotation_cases(BridgeFunction build){
    // Authored raw encodings exercise both polynomial domains and all three
    // reused angles. The fingerprint includes every output matrix and rejects.
    constexpr std::array<std::uint32_t,32> edges{
        0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,0x00800000,0x80800000,
        0x3fc90fda,0x3fc90fdb,0xbfc90fdb,0x40490fdb,0xc0490fdb,
        0x40ffffff,0x41000000,0xc1000000,0x41000001,0xc1000001,
        0x417fffff,0x41800000,0x41800001,0xc17fffff,0xc1800000,0xc1800001,
        0x497fffff,0x49800000,0xc9800000,0x49800001,0xc9800001,
        0x7f800000,0xff800000,0x7fc12345};
    std::uint32_t random=0x93712a5u;std::uint64_t hash=14695981039346656037ull;
    unsigned accepted=0,rejected=0;
    const auto sample=[&](float scale){random=random*1664525u+1013904223u;
        return float(std::int32_t(random%65537)-32768)*scale;};
    const auto bits=[](float f){return std::bit_cast<std::uint32_t>(f);};
    const auto add=[&](std::uint32_t word){for(unsigned i=0;i<4;++i){hash^=(word>>(8*i))&255;hash*=1099511628211ull;}};
    for(unsigned n=0;n<512;++n){
        RenderPose pose;CarRenderGeometry geometry;AffineTransform3 view;
        for(auto& x:pose.words.position)x=bits(sample(1.f/64));
        for(auto& x:pose.words.orientation)x=bits(sample(1.f/8192));
        pose.words.attributes[0]=bits(sample(1.f/8192));
        pose.words.attributes[2]=bits(sample(32));
        for(unsigned i=3;i<11;++i)pose.words.attributes[i]=bits(sample(1.f/32768));
        for(auto& p:geometry.wheel_offsets)p={sample(1.f/8192),sample(1.f/32768),sample(1.f/8192)};
        for(auto& p:view.columns)p={sample(1.f/16384),sample(1.f/16384),sample(1.f/16384)};
        view.translation={sample(1.f/64),sample(1.f/64),sample(1.f/64)};
        if(n<96){const auto edge=edges[n%32];
            if(n<32)pose.words.attributes[2]=edge;
            else if(n<64)pose.words.attributes[0]=edge;
            else pose.words.orientation[1]=edge;
        }else if(n<128){
            // All inputs pass their individual domains, but the derived shadow
            // roll exceeds its domain after the body and wheels were staged.
            pose.words.attributes[7]=pose.words.attributes[9]=0x49800000;
            pose.words.attributes[8]=pose.words.attributes[10]=0xc9800000;
        }
        std::array<std::uint32_t,72> sentinel;
        sentinel.fill(0xa5a5a5a5u);
        auto actual=std::bit_cast<CarRenderBridge>(sentinel);
        static_assert(sizeof actual==72*sizeof(std::uint32_t));
        const auto before=actual;
        const bool okay=build(pose,geometry,view,actual);
        if(okay)++accepted;else{++rejected;require(same(actual,before));}
        add(okay);for(auto word:std::bit_cast<std::array<std::uint32_t,72>>(actual))add(word);
        if(n<128){
            // Every legal matrix subobject may alias the staged destination,
            // including calls that reject only after several rotations.
            for(unsigned part=0;part<6;++part){auto aliased=before;
                auto& input=part==0?aliased.shadow:part==1?aliased.body:aliased.wheels[part-2];
                input=view;const auto alias_before=aliased;
                require(build(pose,geometry,input,aliased)==okay);
                require(same(aliased,okay?actual:alias_before));
            }
        }
    }
    require(accepted==443&&rejected==69);
    std::printf("RENDER_BRIDGE_ROTATIONS cases=512 accepted=%u rejected=%u hash=%016llx\n",
        accepted,rejected,(unsigned long long)hash);
    return hash;
}
}
int main(){try{
    const auto rotation_hash=rotation_cases(build_car_render_bridge);
    require(rotation_hash==0x9ef0ab3306a9f778ull);
    RenderPose pose;CarRenderGeometry geometry;
    geometry.wheel_offsets={AffinePoint3{-1,0,1},AffinePoint3{1,0,1},AffinePoint3{-1,0,-1},AffinePoint3{1,0,-1}};
    const auto view=identity();CarRenderBridge bridge;
    require(build_car_render_bridge(pose,geometry,view,bridge));
    const auto baseline=bridge;
    // All output aliases consume their input matrix before any publication.
    bridge.body=view;require(build_car_render_bridge(pose,geometry,bridge.body,bridge));require(same(bridge,baseline));
    bridge.shadow=view;require(build_car_render_bridge(pose,geometry,bridge.shadow,bridge));require(same(bridge,baseline));
    for(unsigned axis=0;axis<3;++axis){auto bad=pose;bad.words.position[axis]=0x7fc12345;
        const auto before=bridge;require(!build_car_render_bridge(bad,geometry,view,bridge));require(same(bridge,before));}
    for(unsigned attribute:{0u,2u,3u,4u,5u,6u,7u,8u,9u,10u}){
        auto bad=pose;bad.words.attributes[attribute]=0x7f800000;const auto before=bridge;
        require(!build_car_render_bridge(bad,geometry,view,bridge));require(same(bridge,before));
    }
    for(unsigned wheel=0;wheel<4;++wheel){auto bad=geometry;bad.wheel_offsets[wheel].z=value(0x42800001);
        const auto before=bridge;require(!build_car_render_bridge(pose,bad,view,bridge));require(same(bridge,before));}
    auto unused=pose;unused.words.motion.fill(0xffffffff);unused.words.attributes[1]=0x7fc12345;unused.flags=~0u;unused.progress=~0u;
    require(build_car_render_bridge(unused,geometry,view,bridge));require(same(bridge,baseline));
    constexpr std::array<std::uint32_t,14> phases{0,0x80000000,1,0x80000001,0x00800000,0x80800000,
        0x41800000,0xc1800000,0x42700000,0xc2700000,0x497fffff,0xc97fffff,0x49800000,0xc9800000};
    for(auto phase:phases){pose.words.attributes[2]=phase;require(build_car_render_bridge(pose,geometry,view,bridge));}
    for(auto phase:{0x49800001u,0xc9800001u}){pose.words.attributes[2]=phase;const auto before=bridge;
        require(!build_car_render_bridge(pose,geometry,view,bridge));require(same(bridge,before));}
    pose={};DrivingRenderView output;DrivingRenderViewState state;DrivingRenderViewConfig config;
    require(update_driving_render_view(pose,config,state,output));
    require(output.focus_x==300&&output.focus_y==300);
    const auto old_output=output;
    for(unsigned axis=0;axis<3;++axis){auto bad=pose;bad.words.orientation[axis]=0x7fc12345;
        const auto old_state=state;require(!update_driving_render_view(bad,config,state,output));
        require(same(old_state,state)&&same(old_output,output));}
    auto old_state=state;require(!update_driving_render_view(pose,{value(0x42800001)},state,output));
    require(same(old_state,state)&&same(old_output,output));
    state.yaw_history=value(0x40490fdc);old_state=state;
    require(!update_driving_render_view(pose,config,state,output));require(same(old_state,state)&&same(old_output,output));
    // Persistent history changes the next view; no implicit per-tick reset.
    state={};pose.words.orientation[1]=0x3f000000;
    require(update_driving_render_view(pose,config,state,output));const auto first=state;
    require(update_driving_render_view(pose,config,state,output));require(state.yaw_history!=first.yaw_history);
    DrivingRenderViewState reset{};DrivingRenderView reset_view;
    require(update_driving_render_view(pose,config,reset,reset_view));require(reset.yaw_history==first.yaw_history);
    require(!same(output.world_to_view,reset_view.world_to_view));
    // Signed-zero and near-pi inputs exercise the one-fold boundary without a
    // host trigonometric replacement or narrowing the supported raw domain.
    for(auto angle:{0u,0x80000000u,1u,0x80000001u,0x40490fdau,0x40490fdbu,0xc0490fdau,0xc0490fdbu}){
        pose={};pose.words.orientation[1]=angle;state.yaw_history=value(angle);
        require(update_driving_render_view(pose,config,state,output));
    }
    std::printf("RENDER_BRIDGE PASS checks=%u\n",checks);
}catch(unsigned n){std::fprintf(stderr,"RENDER_BRIDGE FAIL check=%u\n",n);return 1;}return 0;}
