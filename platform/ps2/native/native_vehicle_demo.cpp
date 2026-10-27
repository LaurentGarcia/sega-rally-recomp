#include "native_vehicle_demo.h"
#include "main_vehicle_checkpoint.h"
#include "rally_body_scene.h"
#include "debug_controls.h"
#include "vu1_transform.h"
#include <timer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace ps2::native {
namespace {
constexpr WorldMatrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct VuPresentation {
    ps2::Vu1Transform unit;
    bool native_float=false;
    struct Counts {
        unsigned near_plane=0,budget=0,invalid=0,unsupported=0;
        std::uint64_t transform_ticks=0,guard_ticks=0;
        double max_pixels=0,max_depth=0;
    } counts;
    static bool batch(void* context,const WorldMatrix& object,const WorldMatrix& camera,
                      std::span<const ps2::TransformVector4> input,std::span<ps2::TransformVector4> output,
                      std::span<std::uint8_t> accepted) noexcept {
        auto& self=*static_cast<VuPresentation*>(context);
        std::fill(accepted.begin(),accepted.end(),0);
        if(!self.unit.available())return false;
        if(self.native_float){
            const auto started=GetTimerSystemTime();
            const bool completed=self.unit.transform_native(object,camera,input,output);
            self.counts.transform_ticks+=GetTimerSystemTime()-started;
            if(completed)std::fill(accepted.begin(),accepted.end(),1);
            else if(self.unit.available())self.counts.unsupported+=unsigned(input.size());
            return completed||self.unit.available();
        }
        ps2::TransformErrorBound bound;
        auto started=GetTimerSystemTime();
        const bool completed=self.unit.transform(object,camera,input,output,bound);
        self.counts.transform_ticks+=GetTimerSystemTime()-started;
        if(!completed){
            // Unsupported magnitudes use the reference path; transport failure
            // retires the owner and is separately reported by failed_batches.
            if(self.unit.available()){self.counts.unsupported+=unsigned(input.size());return true;}
            return false;
        }
        started=GetTimerSystemTime();
        ps2::PreparedProjectionGuard guard;
        if(!ps2::prepare_vu1_projection_guard(bound,{},guard)){
            self.counts.invalid+=unsigned(input.size());
            self.counts.guard_ticks+=GetTimerSystemTime()-started;return true;
        }
        for(std::size_t i=0;i<input.size();++i){
            ps2::TransformProjectionError estimate;
            switch(ps2::guard_prepared_vu1_projection(output[i],guard,&estimate)){
            case ps2::TransformGuard::accepted:
                accepted[i]=1;
                self.counts.max_pixels=std::max(self.counts.max_pixels,estimate.pixels);
                self.counts.max_depth=std::max(self.counts.max_depth,estimate.depth);break;
            case ps2::TransformGuard::near_clip_plane:++self.counts.near_plane;break;
            case ps2::TransformGuard::projection_budget:++self.counts.budget;break;
            case ps2::TransformGuard::invalid:++self.counts.invalid;break;
            }
        }
        self.counts.guard_ticks+=GetTimerSystemTime()-started;return true;
    }
};
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
                            const char* course_path,const char* body_path,NativeVehicleRenderMode mode) {
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
    body.set_profile_clock([](void*) noexcept -> std::uint64_t {return GetTimerSystemTime();});
    VuPresentation vu;
    bool use_vu1=false;
    const auto mode_name=[&](){return vu.native_float?(use_vu1?"NATIVE_FLOAT_VU1":"NATIVE_FLOAT_EE"):
        (use_vu1?"GUARDED_VU1":"EXACT_EE");};
    const auto apply_mode=[&](){
        vu.native_float=mode==NativeVehicleRenderMode::native_vu;
        body.set_numeric_mode(vu.native_float?WorldNumericMode::native_float:WorldNumericMode::reference);
        use_vu1=mode!=NativeVehicleRenderMode::reference&&vu.unit.initialize();
        body.set_transform_provider(use_vu1?VuPresentation::batch:nullptr,&vu);
        std::printf("NATIVE_VEHICLE_VU mode=%s status=\"%s\" synchronous=1\n",mode_name(),vu.unit.error());
    };
    apply_mode();
    auto state=initial.state;auto controls=initial.input;
    const float camera_yaw=state.front.contact.vehicle.orientation[1]+.35f;
    MainVehicleOutput result;ps2::DebugPageToggle debug_toggle;
    bool diagnostics=false;std::uint16_t previous_buttons=0;
    ps2::PadSnapshot previous_pad;const char* previous_pad_status="";
    int previous_sdk_state=-1,previous_sdk_request=-1;
    char page[1500]{};std::uint64_t total_update_ticks=0,total_draw_ticks=0;
    display.set_caption("NATIVE VEHICLE TEST\nDIAGNOSTIC BODY / COURSE");
    std::printf("NATIVE_VEHICLE_BEGIN course=%lu asset_crc=%08lx checkpoint_frame=%lu body_bytes=%lu\n",
        (unsigned long)assets.selector(),(unsigned long)assets.payload_crc32(),(unsigned long)controls.frame,(unsigned long)body.persistent_bytes());
    for(unsigned frame=1;;++frame){
        auto pad_state=pad.poll();
        const char* pad_status=pad.status();
        if(frame<=3||frame%60==0||std::strcmp(pad_status,previous_pad_status)!=0||
           pad_state.connected!=previous_pad.connected||pad_state.analog!=previous_pad.analog||
           pad_state.buttons!=previous_pad.buttons||pad_state.left_x!=previous_pad.left_x||
           pad.sdk_state()!=previous_sdk_state||pad.sdk_request_state()!=previous_sdk_request)
            std::printf("NATIVE_VEHICLE_PAD update=%u connected=%u analog=%u buttons=%04x left_x=%u sdk_state=%d sdk_request=%d status=\"%s\"\n",
                frame,unsigned(pad_state.connected),unsigned(pad_state.analog),unsigned(pad_state.buttons),
                unsigned(pad_state.left_x),pad.sdk_state(),pad.sdk_request_state(),pad_status);
        previous_pad=pad_state;previous_pad_status=pad_status;
        previous_sdk_state=pad.sdk_state();previous_sdk_request=pad.sdk_request_state();
        if(debug_toggle.update(pad_state))diagnostics=!diagnostics;
        const auto pressed=pad_state.buttons&~previous_buttons;previous_buttons=pad_state.buttons;
        if(pressed&ps2::Triangle){
            mode=static_cast<NativeVehicleRenderMode>((unsigned(mode)+1)%3);apply_mode();
        }
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
        vu.counts={};
        if(!body.update(pose,view(car.position,camera_yaw),body_error))throw std::runtime_error(rally_body_error_message(body_error));
        if(use_vu1&&!vu.unit.available()){
            use_vu1=false;body.set_transform_provider(nullptr);
            std::printf("NATIVE_VEHICLE_VU_RETIRED update=%u status=\"%s\" switching_to=%s\n",frame,vu.unit.error(),mode_name());
        }
        const auto geometry_ticks=GetTimerSystemTime()-draw_start;
        if(diagnostics)std::snprintf(page,sizeof page,
            "NATIVE VEHICLE DEVELOPMENT TEST\n"
            "ONE INITIAL STATE / LIVE PAD\n"
            "RECOVERED CONTACTS AND MOTION\n"
            "DIAGNOSTIC BODY / COLLISION COURSE\n"
            "CAMERA AND BODY AXIS BRIDGE DIAGNOSTIC\n"
            "GAME AUDIO AND GAME FLOW NOT CONNECTED\n\n"
            "PAD %s / UPDATE %u\n"
            "PAD STATUS %s\n"
            "SDK STATE %d / REQUEST %d\n"
            "GEAR %lu / SEGMENT %d\n"
            "POSITION %.2f %.2f %.2f\n"
            "MOTION %.3f %.3f %.3f\n"
            "NATIVE UPDATE %llu US\n"
            "GEOMETRY %llu US / TRIANGLES %lu\n\n"
            "%s / VU1 VERTICES %lu\n"
            "CROSS GAS / SQUARE BRAKE\n"
            "STICK OR LEFT/RIGHT STEER\n"
            "START RESET INITIAL CHECKPOINT\n"
            "TRIANGLE CYCLE RENDERER\n"
            "SELECT+START TOGGLE THIS PAGE\n"
            "TIMING IS EMULATOR OBSERVATION",
            pad_state.connected?"CONNECTED":"DISCONNECTED",frame,pad_status,pad.sdk_state(),pad.sdk_request_state(),(unsigned long)state.gear,int(state.segment),
            double(car.position[0]),double(car.position[1]),double(car.position[2]),double(car.motion[0]),double(car.motion[1]),double(car.motion[2]),
            (unsigned long long)(update_ticks*1000000/kBUSCLK),(unsigned long long)(geometry_ticks*1000000/kBUSCLK),
            (unsigned long)body.stats().output_triangles,mode_name(),
            (unsigned long)body.stats().accepted_batch_vertices);
        display.set_debug_page(diagnostics?page:nullptr);
        if(!display.present(body.frame()))throw std::runtime_error(display.error());
        const auto draw_ticks=GetTimerSystemTime()-draw_start;total_draw_ticks+=draw_ticks;
        if(frame<=3||frame%60==0){
            const auto stats=body.stats();
            std::printf("NATIVE_VEHICLE_GEOMETRY update=%u validation_ticks=%llu bounds_ticks=%llu transform_ticks=%llu clipping_ticks=%llu transformed_vertices=%lu cache_hits=%lu groups=%lu culled_groups=%lu\n",
                frame,(unsigned long long)stats.validation_ticks,(unsigned long long)stats.bounds_ticks,
                (unsigned long long)stats.transform_ticks,(unsigned long long)stats.clipping_ticks,
                (unsigned long)stats.transformed_vertices,(unsigned long)stats.vertex_cache_hits,
                (unsigned long)stats.groups,(unsigned long)stats.culled_groups);
            std::printf("NATIVE_VEHICLE_VU_FRAME update=%u batches=%lu vertices=%lu accepted=%lu used_corners=%lu clipped_fallbacks=%lu failed_batches=%lu near_plane=%u budget=%u invalid=%u unsupported=%u transform_ticks=%llu guard_ticks=%llu max_bound_pixels=%.7g max_bound_depth=%.7g vu0_vertices=%lu status=\"%s\"\n",
                frame,(unsigned long)stats.batch_calls,(unsigned long)stats.batch_vertices,
                (unsigned long)stats.accepted_batch_vertices,(unsigned long)stats.provider_triangle_corners,
                (unsigned long)stats.clipped_batch_fallbacks,(unsigned long)stats.failed_batches,
                vu.counts.near_plane,vu.counts.budget,vu.counts.invalid,vu.counts.unsupported,
                (unsigned long long)vu.counts.transform_ticks,(unsigned long long)vu.counts.guard_ticks,
                vu.counts.max_pixels,vu.counts.max_depth,(unsigned long)display.stats().vu0_vertices,vu.unit.error());
        }
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
