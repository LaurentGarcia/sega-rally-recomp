#include "native_vehicle_demo.h"
#include "main_vehicle_checkpoint.h"
#include "rally_body_scene.h"
#include "debug_controls.h"
#include <timer.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace ps2::native {
namespace {
constexpr WorldMatrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
WorldView view(const std::array<float,3>& target,float yaw) {
    // Explicit orthographic developer camera. No reference-view matrix or
    // fitted mesh bounds participate. The course remains fixed in world space.
    const float s=std::sin(yaw),c=std::cos(yaw),pitch_s=.5f,pitch_c=.8660254f;
    const std::array<float,3> right{c,0,-s},up{s*pitch_s,pitch_c,c*pitch_s},forward{s*pitch_c,-pitch_s,c*pitch_c};
    WorldView result;
    for(unsigned i=0;i<3;++i){result.world_to_clip[4*i]=right[i]/7;
        result.world_to_clip[4*i+1]=up[i]/5.42f;result.world_to_clip[4*i+2]=forward[i]/100;}
    for(unsigned i=0;i<3;++i){result.world_to_clip[12]-=right[i]*target[i]/7;
        result.world_to_clip[13]-=up[i]*target[i]/5.42f;result.world_to_clip[14]-=forward[i]*target[i]/100;}
    result.world_to_clip[14]+=.5f;result.world_to_clip[15]=1;
    return result;
}
}
int run_native_vehicle_demo(Display& display,ps2::PadInput& pad,const char* checkpoint_path,
                            const char* course_path,const char* body_path) {
    display.present_status("NATIVE VEHICLE DEVELOPMENT TEST\nLOADING ONE INITIAL CHECKPOINT\nLIVE PAD / NATIVE GAME UPDATE\nDIAGNOSTIC BODY AND COURSE VIEW");
    DrivingAssets assets;DrivingAssetError asset_error;
    if(!load_driving_assets_file(course_path,assets,asset_error))throw std::runtime_error(driving_asset_error_message(asset_error));
    MainVehicleCheckpoint initial;MainVehicleCheckpointError checkpoint_error;
    if(!load_main_vehicle_checkpoint_file(checkpoint_path,initial,checkpoint_error))throw std::runtime_error(main_vehicle_checkpoint_error_message(checkpoint_error));
    if(initial.course_selector!=assets.selector()||initial.asset_payload_crc32!=assets.payload_crc32())
        throw std::runtime_error("CHECKPOINT AND COURSE ASSETS DIFFER");
    if(initial.input.operating_mode!=1)throw std::runtime_error("DRIVING TEST REQUIRES ACTIVE RACE CHECKPOINT");
    RallyBodyScene body;RallyBodyError body_error;
    if(!body.load(body_path,identity,body_error)||!body.attach_course(assets.course(),body_error))
        throw std::runtime_error(rally_body_error_message(body_error));
    auto state=initial.state;auto controls=initial.input;
    const float camera_yaw=state.front.contact.vehicle.orientation[1]+.35f;
    MainVehicleOutput result;ps2::DebugPageToggle debug_toggle;
    bool diagnostics=false;std::uint16_t previous_buttons=0;
    char page[1500]{};std::uint64_t total_update_ticks=0,total_draw_ticks=0;
    display.set_caption("NATIVE VEHICLE TEST\nDIAGNOSTIC BODY / COURSE");
    std::printf("NATIVE_VEHICLE_BEGIN course=%lu asset_crc=%08lx checkpoint_frame=%lu body_bytes=%lu\n",
        (unsigned long)assets.selector(),(unsigned long)assets.payload_crc32(),(unsigned long)controls.frame,(unsigned long)body.persistent_bytes());
    for(unsigned frame=1;;++frame){
        auto pad_state=pad.poll();
        if(debug_toggle.update(pad_state))diagnostics=!diagnostics;
        const auto pressed=pad_state.buttons&~previous_buttons;previous_buttons=pad_state.buttons;
        if((pressed&ps2::Start)&&!(pad_state.buttons&ps2::Select)){state=initial.state;controls=initial.input;}
        controls.steering_sample=128;controls.accelerator_sample=48;controls.brake_sample=48;
        if(pad_state.connected){
            if(pad_state.analog)controls.steering_sample=std::uint8_t(128+(int(pad_state.left_x)-128)*80/128);
            if(pad_state.buttons&ps2::Left)controls.steering_sample=48;
            if(pad_state.buttons&ps2::Right)controls.steering_sample=208;
            if(pad_state.buttons&ps2::Cross)controls.accelerator_sample=191;
            if(pad_state.buttons&ps2::Square)controls.brake_sample=176;
            if(controls.manual_selection){
                if((pressed&ps2::R1)&&controls.requested_gear<4)++controls.requested_gear;
                if((pressed&ps2::L1)&&controls.requested_gear>1)--controls.requested_gear;
            }
        }
        const auto update_start=GetTimerSystemTime();
        if(!update_main_vehicle(assets,initial.setup,controls,state,result)){
            std::printf("NATIVE_VEHICLE_REJECT update=%u game_frame=%lu\n",frame,(unsigned long)controls.frame);
            throw std::runtime_error("NATIVE VEHICLE INPUT OUTSIDE VALIDATED DOMAIN");
        }
        const auto update_ticks=GetTimerSystemTime()-update_start;total_update_ticks+=update_ticks;
        const auto draw_start=GetTimerSystemTime();
        const auto& car=state.front.contact.vehicle;
        RallyBodyPose pose{{car.position[0],car.position[1],car.position[2]},
                           {car.orientation[0],car.orientation[1],car.orientation[2]}};
        if(!body.update(pose,view(car.position,camera_yaw),body_error))throw std::runtime_error(rally_body_error_message(body_error));
        const auto geometry_ticks=GetTimerSystemTime()-draw_start;
        std::snprintf(page,sizeof page,
            "NATIVE VEHICLE DEVELOPMENT TEST\n"
            "ONE INITIAL STATE / LIVE PAD\n"
            "RECOVERED CONTACTS AND MOTION\n"
            "DIAGNOSTIC BODY / COLLISION COURSE\n"
            "CAMERA AND BODY AXIS BRIDGE DIAGNOSTIC\n"
            "GAME AUDIO AND GAME FLOW NOT CONNECTED\n\n"
            "PAD %s / UPDATE %u\n"
            "GEAR %lu / SEGMENT %d\n"
            "POSITION %.2f %.2f %.2f\n"
            "MOTION %.3f %.3f %.3f\n"
            "NATIVE UPDATE %llu US\n"
            "GEOMETRY %llu US / TRIANGLES %lu\n\n"
            "CROSS GAS / SQUARE BRAKE\n"
            "STICK OR LEFT/RIGHT STEER\n"
            "START RESET INITIAL CHECKPOINT\n"
            "SELECT+START TOGGLE THIS PAGE\n"
            "TIMING IS EMULATOR OBSERVATION",
            pad_state.connected?"CONNECTED":"DISCONNECTED",frame,(unsigned long)state.gear,int(state.segment),
            double(car.position[0]),double(car.position[1]),double(car.position[2]),double(car.motion[0]),double(car.motion[1]),double(car.motion[2]),
            (unsigned long long)(update_ticks*1000000/kBUSCLK),(unsigned long long)(geometry_ticks*1000000/kBUSCLK),
            (unsigned long)body.stats().output_triangles);
        display.set_debug_page(diagnostics?page:nullptr);
        if(!display.present(body.frame()))throw std::runtime_error(display.error());
        const auto draw_ticks=GetTimerSystemTime()-draw_start;total_draw_ticks+=draw_ticks;
        if(frame<=3||frame%60==0)std::printf(
            "NATIVE_VEHICLE_FRAME update=%u game_frame=%lu pad=%u controls=%u,%u,%u position=%.7g,%.7g,%.7g motion=%.7g,%.7g,%.7g update_ticks=%llu geometry_ticks=%llu display_ticks=%llu total_update_ticks=%llu total_draw_ticks=%llu frequency=%u triangles=%lu sound_requests=%u\n",
            frame,(unsigned long)controls.frame,unsigned(pad_state.connected),unsigned(controls.steering_sample),unsigned(controls.accelerator_sample),unsigned(controls.brake_sample),
            double(car.position[0]),double(car.position[1]),double(car.position[2]),double(car.motion[0]),double(car.motion[1]),double(car.motion[2]),
            (unsigned long long)update_ticks,(unsigned long long)geometry_ticks,(unsigned long long)draw_ticks,
            (unsigned long long)total_update_ticks,(unsigned long long)total_draw_ticks,unsigned(kBUSCLK),
            (unsigned long)body.stats().output_triangles,result.command_count);
        ++controls.frame;
    }
}
} // namespace ps2::native
