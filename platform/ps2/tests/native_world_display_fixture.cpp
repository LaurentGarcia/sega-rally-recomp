// Original authored geometry: complete Frame/PATH3 versus world/PATH1 pixels.
// No game assets, instruction runtime or captured transformed vertices.
#include "native_display.h"
#include "world_scene.h"
#include "vu1_transform.h"
#include "vu1_world.h"
#include <dma_registers.h>
#include <gs_gp.h>
#include <gs_privileged.h>
#include <kernel.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
using namespace ps2::native;
constexpr unsigned face_count=35;
std::array<std::uint32_t,640*512> reference;
std::array<std::uint32_t,640*16> stripe;
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void require(bool value,const char* why){if(!value){std::printf("WORLD DISPLAY CHECK FAILED: %s\n",why);throw std::runtime_error(why);}}
struct Authored {
    std::array<std::uint8_t,32> pixels0{},pixels1{};
    Texture texture0{pixels0.data(),pixels0.size(),8,8,101};
    Texture texture1{pixels1.data(),pixels1.size(),8,8,102};
    std::array<Material,5> materials{};
    std::array<WorldPoint,face_count*3> vertices{};
    std::array<WorldFace,face_count> faces{};
    Authored(){
        for(unsigned i=0;i<32;++i){pixels0[i]=std::uint8_t(i*9);pixels1[i]=std::uint8_t(255-i*7);}
        for(unsigned m=0;m<materials.size();++m){auto& material=materials[m];
            material.texture=(m&1)?&texture1:&texture0;
            material.cutout=(m==1||m==3);material.checker=m>=3;
            for(unsigned p=0;p<16;++p)material.palette[p]=0xff000000u|((p*13+m*17)<<16)|((p*7+m*31)<<8)|(p*11+m*3);
            material.palette[0]&=0x00ffffffu;
        }
        for(unsigned i=0;i<face_count;++i){const unsigned cell=i%16;
            const float x=-.875f+float(cell%4)*.4375f,y=-.875f+float(cell/4)*.4375f;
            const float z=i<16?.75f:.25f;
            vertices[i*3]={x,y,z};vertices[i*3+1]={x+.3125f,y,z};vertices[i*3+2]={x+.125f,y+.3125f,z};
            auto& face=faces[i];face.indices={i*3,i*3+1,i*3+2};face.argb=0xffc04020u+i*0x101u;
            if(i%6)face.material_index=(i%6)-1;
            // UV seams are per corner, with repeats and both signs of texels.
            const float u=float(int(i%3)-1)*8;
            face.uv={{{u,0},{u+8,0},{u+4,8}}};
        }
        // Second batch contains every canonical side and the near/far planes.
        vertices[16*3].z=-.25f;vertices[17*3+1].x=1.25f;
        vertices[18*3].x=-1.25f;vertices[19*3+2].y=1.25f;
        vertices[20*3].y=-1.25f;vertices[21*3+2].z=1.25f;
        // Third batch cannot emit a prefix: one triangle has zero area.
        // Model admission requires distinct indices; coincident positions
        // produce the intended geometric degeneracy after valid admission.
        vertices[33*3+2]=vertices[33*3+1];
    }
    WorldModel model(){return {vertices,faces,{},materials};}
};
struct Eviction {
    static constexpr unsigned count=288;
    std::array<std::uint8_t,256*256/2> pixels{};
    std::array<Texture,32> textures{};
    std::array<Material,count> materials{};
    std::array<WorldPoint,count*3> vertices{};
    std::array<WorldFace,count> faces{};
    Eviction(){
        for(unsigned i=0;i<textures.size();++i)textures[i]={pixels.data(),pixels.size(),256,256,1000+i};
        for(unsigned i=0;i<count;++i){
            const float x=-1.f+float(i%32)/16.f,y=-.875f+float(i/32)*.1875f;
            vertices[i*3]={x,y,.5f};vertices[i*3+1]={x+.046875f,y,.5f};vertices[i*3+2]={x,y+.125f,.5f};
            faces[i].indices={i*3,i*3+1,i*3+2};faces[i].material_index=i;
            faces[i].uv={{{0,0},{8,0},{0,8}}};materials[i].texture=&textures[i%32];
            materials[i].palette.fill(0xff000000u|((i+1)*0x103u));
        }
    }
    WorldModel model(){return {vertices,faces,{},materials};}
};
struct OutsideTextures {
    std::array<std::uint8_t,32> pixels{};
    std::array<Texture,15> textures{};
    std::array<Material,15> materials{};
    std::array<WorldPoint,48> vertices{};
    std::array<WorldFace,16> faces{};
    OutsideTextures(){
        pixels.fill(0x11);
        for(unsigned i=0;i<16;++i){
            const float x=i?2.f+float(i)*.125f:-.5f;
            vertices[i*3]={x,-.5f,.5f};vertices[i*3+1]={x+.25f,-.5f,.5f};
            vertices[i*3+2]={x,-.25f,.5f};faces[i].indices={i*3,i*3+1,i*3+2};
            faces[i].argb=0xffc06020u;
            if(i){const auto m=i-1;textures[m]={pixels.data(),pixels.size(),8,8,5000+i};
                materials[m].texture=&textures[m];materials[m].palette.fill(0xff000000u|((i*17)<<16)|(i*13));
                faces[i].material_index=m;faces[i].uv={{{0,0},{8,0},{0,8}}};}
        }
    }
    WorldModel model(){return {vertices,faces,{},materials};}
};
struct MemoMaterials {
    static constexpr unsigned count=400;
    // These index pairs cover every4-byte-aligned EE Material-array base for
    // the64-entry pointer hash. Earlier faces establish the compacted index
    // order; each final16-face batch alternates A/B to stress state re-lookup.
    static constexpr std::array<std::array<unsigned,2>,13> collision_pairs{{
        {{1,54}},{{50,103}},{{12,65}},{{51,104}},{{35,88}},{{69,122}},{{10,63}},
        {{4,57}},{{68,121}},{{67,120}},{{9,62}},{{58,111}},{{66,119}}}};
    std::array<std::uint8_t,128> pixels0{},pixels1{};
    std::array<Texture,2> textures{};
    std::array<Material,128> materials{};
    std::array<WorldPoint,count*3> vertices{};
    std::array<WorldFace,count> faces{};
    MemoMaterials(){
        pixels0.fill(0x21);pixels1.fill(0x43);
        textures={Texture{pixels0.data(),pixels0.size(),8,8,9001},Texture{pixels1.data(),pixels1.size(),8,8,9002}};
        for(unsigned i=0;i<materials.size();++i){
            materials[i].texture=&textures[i&1];materials[i].cutout=(i%7)==0;materials[i].checker=(i%9)==0;
            for(unsigned c=0;c<16;++c)materials[i].palette[c]=0xff000000u|((i+1)*0x107u+c*0x110100u);
        }
        materials[1]=materials[0]; // Different immutable pointers, equal resources/state.
        for(unsigned i=0;i<count;++i){
            const float x=-.9375f+float(i%16)*.1171875f,y=-.9375f+float((i/16)%12)*.15625f;
            vertices[i*3]={x,y,.5f};vertices[i*3+1]={x+.078125f,y,.5f};vertices[i*3+2]={x,y+.109375f,.5f};
            auto& f=faces[i];f.indices={3*i,3*i+1,3*i+2};
            f.material_index=i<16?0:i<32?((i&1)?(i-16)/2+1:0):(i-32)%128;
            if(i>=192)f.material_index=collision_pairs[(i-192)/16][i&1];
            f.uv={{{0,0},{8,0},{0,8}}};
        }
    }
    WorldModel model(){return {vertices,faces,{},materials};}
};
auto material_counts(const Stats& s){
    return std::array<std::uint64_t,17>{s.triangles,s.texture_uploads,s.texture_hits,s.palette_uploads,s.palette_hits,
        s.texture_bytes,s.palette_bytes,s.texture_evictions,s.texture_pages,s.checker_triangles,
        s.world_source_triangles,s.world_batches,s.world_drawn_batches,s.world_fallback_batches,
        s.world_path3_handoffs,s.world_path3_stream_retirements,s.world_gs_finishes};
}
auto fallback_control_counts(const Stats& s){
    return std::array<std::uint64_t,34>{s.triangles,s.texture_uploads,s.texture_hits,s.palette_uploads,s.palette_hits,
        s.texture_bytes,s.palette_bytes,s.vu0_vertices,s.checker_triangles,s.texture_evictions,s.texture_pages,
        s.world_source_triangles,s.world_batches,s.world_drawn_batches,s.world_fallback_batches,s.world_rejected_batches,
        s.world_vu1_triangles,s.world_path3_handoffs,s.world_path3_empty_skips,s.world_gs_finishes,
        s.world_path3_stream_retirements,s.world_prepared_batches,s.world_ready_batches,s.world_residency_fallback_batches,
        s.world_emit_calls,s.world_skipped_preload_materials,s.world_async_prepare_submissions,s.world_async_emit_submissions,
        s.world_async_gathered_triangles,s.world_material_memo_hits,s.world_material_state_hits,s.world_material_residency_fast,
        s.world_material_memo_misses,s.world_material_memo_invalidations};
}
std::uint64_t framebuffer(Display& display,bool compare,unsigned phase,std::uint64_t& words){
    require(display.output_height()<=512,"UNSUPPORTED FIXTURE HEIGHT");
    std::uint64_t hash=1469598103934665603ull;unsigned different=0;
    for(unsigned y=0;y<display.output_height();y+=16){
        const unsigned rows=std::min(16u,display.output_height()-y),count=rows*640;
        const auto output=std::span(stripe).first(count);
        const auto status=display.readback_rect(0,y,640,rows,output);
        if(status!=ReadbackStatus::ok){std::printf("WORLD DISPLAY READBACK FAIL phase=%u y=%u status=%u reason=%s\n",phase,y,unsigned(status),display.error());throw std::runtime_error("FRAMEBUFFER TRANSPORT");}
        for(unsigned i=0;i<count;++i){const auto offset=y*640+i;const auto value=stripe[i];
            if(compare&&reference[offset]!=value){if(different<8)std::printf("WORLD DISPLAY PIXEL phase=%u x=%u y=%u expected=%08lx actual=%08lx\n",phase,i%640,y+i/640,(unsigned long)reference[offset],(unsigned long)value);++different;}
            if(!compare)reference[offset]=value;
            hash=(hash^value)*1099511628211ull;
        }
        if(compare)words+=count;
    }
    require(!different,"FRAME/PATH1 PIXEL DIFFERENCE");return hash;
}
unsigned token_checks(ps2::Vu1Transform& unit,unsigned height){
    using namespace ps2;
    unsigned checks=0;
    const auto check=[&](bool okay,const char* label){require(okay,label);++checks;};
    const std::array<Vu1WorldCorner,3> corners{{{{-.5f,-.5f,.5f,1},0,0,1,1},
                                              {{.5f,-.5f,.5f,1},0,0,1,1},
                                              {{0,.5f,.5f,1},0,0,1,1}}};
    Vu1WorldGeometry geometry{identity(),identity(),corners,height,700};
    const std::array<TransformVector4,3> sentinel{{{7,8,9,10},{11,12,13,14},{15,16,17,18}}};
    auto clips=sentinel;Vu1WorldToken token;
    check(unit.cancel_world(token,clips)==Vu1WorldResult::rejected,"DEFAULT TOKEN ACCEPTED");
    check(std::memcmp(clips.data(),sentinel.data(),sizeof clips)==0,"REJECTED TOKEN CHANGED CLIPS");
    check(unit.prepare_world(geometry,token,clips)==Vu1WorldPrepareResult::ready,"TOKEN PREPARATION");
    check(std::memcmp(clips.data(),sentinel.data(),sizeof clips)==0,"READY PREPARATION PUBLISHED CLIPS");
    const auto copy=token;
    check(unit.emit_world(token,{},Vu1WorldCompletion::gs_finished)==Vu1WorldResult::rejected,"SHORT STATE ACCEPTED");
    check(unit.cancel_world(token,std::span(clips).first(2))==Vu1WorldResult::rejected,"SHORT CLIP OUTPUT ACCEPTED");
    Vu1Transform foreign;
    check(foreign.cancel_world(token,clips)==Vu1WorldResult::rejected,"FOREIGN OWNER ACCEPTED TOKEN");
    check(std::memcmp(clips.data(),sentinel.data(),sizeof clips)==0,"INVALID TOKEN ARGUMENTS PUBLISHED CLIPS");
    check(unit.cancel_world(copy,clips)==Vu1WorldResult::fallback,"VALID COPIED TOKEN CANCEL FAILED");
    for(unsigned i=0;i<3;++i)
        check(std::memcmp(&clips[i],&corners[i].position,sizeof clips[i])==0,"CANCEL DID NOT RETAIN EXACT CLIPS");
    const auto retained=clips;
    check(unit.cancel_world(token,clips)==Vu1WorldResult::rejected,"CONSUMED TOKEN ACCEPTED");
    check(std::memcmp(clips.data(),retained.data(),sizeof clips)==0,"CONSUMED TOKEN CHANGED OUTPUT");
    check(unit.prepare_world(geometry,token,clips)==Vu1WorldPrepareResult::ready,"STALE TOKEN PREPARATION");
    const auto stale=token;Vu1WorldToken newer;
    ++geometry.sequence;
    check(unit.prepare_world(geometry,newer,clips)==Vu1WorldPrepareResult::ready,"NEW TOKEN PREPARATION");
    check(unit.cancel_world(stale,clips)==Vu1WorldResult::rejected,"OLDER PREPARATION ACCEPTED");
    check(unit.cancel_world(newer,clips)==Vu1WorldResult::fallback,"NEW TOKEN LOST BY STALE REJECTION");
    check(unit.prepare_world(geometry,token,clips)==Vu1WorldPrepareResult::ready,"INVALIDATION PREPARATION");
    auto invalid=geometry;invalid.corners={};
    check(unit.prepare_world(invalid,newer,clips)==Vu1WorldPrepareResult::rejected,"EMPTY PREPARATION ACCEPTED");
    check(unit.cancel_world(token,clips)==Vu1WorldResult::rejected,"INVALID PREPARATION DID NOT INVALIDATE");
    check(unit.prepare_world(geometry,token,clips)==Vu1WorldPrepareResult::ready,"EMPTY TRANSFORM PREPARATION");
    check(unit.transform_native(identity(),identity(),{},{}),"EMPTY TRANSFORM FAILED");
    check(unit.cancel_world(token,clips)==Vu1WorldResult::rejected,"EMPTY TRANSFORM DID NOT INVALIDATE");
    return checks;
}
unsigned async_pending_polls=0,async_complete_polls=0;
void observe_async_submission(ps2::Vu1Transform& unit,const ps2::Vu1WorldSubmission& submission){
    const auto state=unit.poll_world(submission);
    require(state==ps2::Vu1WorldPollResult::pending||state==ps2::Vu1WorldPollResult::complete,"ASYNC IMMEDIATE POLL FAILED");
    if(state==ps2::Vu1WorldPollResult::pending)++async_pending_polls;else ++async_complete_polls;
}
unsigned async_token_checks(ps2::Vu1Transform& unit,unsigned height){
    using namespace ps2;
    unsigned checks=0;
    const auto check=[&](bool okay,const char* label){require(okay,label);++checks;};
    const std::array<Vu1WorldCorner,3> original{{{{-.5f,-.5f,.5f,1},0,0,1,1},
                                               {{.5f,-.5f,.5f,1},0,0,1,1},
                                               {{0,.5f,.5f,1},0,0,1,1}}};
    auto input=original;
    Vu1WorldGeometry geometry{identity(),identity(),input,height,1000};
    std::array<TransformVector4,3> clips{{{7,8,9,10},{11,12,13,14},{15,16,17,18}}};
    const auto sentinel=clips;
    Vu1WorldSubmission submission,empty;Vu1WorldToken a,b,c;
    check(unit.poll_world(empty)==Vu1WorldPollResult::rejected,"EMPTY SUBMISSION ACCEPTED");
    check(unit.submit_prepare_world(geometry,submission)==Vu1WorldSubmitResult::submitted,"ASYNC FIRST PREPARE");
    observe_async_submission(unit,submission);
    const auto copied=submission;
    // Submission owns a copy: caller scratch can be reused during execution.
    input[0].position.x=.25f;
    check(unit.finish_emit_world(submission)==Vu1WorldResult::rejected,"WRONG FINISH METHOD ACCEPTED");
    check(unit.finish_prepare_world(submission,a,std::span(clips).first(2))==Vu1WorldPrepareResult::rejected,"ASYNC SHORT OUTPUT ACCEPTED");
    check(std::memcmp(clips.data(),sentinel.data(),sizeof clips)==0,"ASYNC SHORT OUTPUT PUBLISHED");
    check(unit.prepare_world(geometry,a,clips)==Vu1WorldPrepareResult::rejected,"SYNCHRONOUS PREPARE ACCEPTED BUSY");
    check(!unit.transform_native(identity(),identity(),{},{}),"EMPTY TRANSFORM ACCEPTED BUSY");
    const auto polled=unit.poll_world(submission);
    check(polled==Vu1WorldPollResult::pending||polled==Vu1WorldPollResult::complete,"LIVE SUBMISSION POLL");
    check(unit.finish_prepare_world(submission,a,clips)==Vu1WorldPrepareResult::ready,"ASYNC FIRST FINISH");
    check(std::memcmp(clips.data(),sentinel.data(),sizeof clips)==0,"ASYNC READY PUBLISHED CLIPS");
    check(unit.finish_prepare_world(copied,c,clips)==Vu1WorldPrepareResult::rejected,"COPIED COMPLETED SUBMISSION ACCEPTED");
    input=original;++geometry.sequence;
    check(unit.submit_prepare_world(geometry,submission)==Vu1WorldSubmitResult::submitted,"ASYNC SECOND BANK PREPARE");
    observe_async_submission(unit,submission);
    check(unit.finish_prepare_world(submission,b,clips)==Vu1WorldPrepareResult::ready,"ASYNC SECOND BANK FINISH");
    ++geometry.sequence;
    check(unit.submit_prepare_world(geometry,submission)==Vu1WorldSubmitResult::rejected,"THIRD RETAINED BANK ACCEPTED");
    check(unit.cancel_world(a,clips)==Vu1WorldResult::fallback,"FIRST BANK LOST AFTER THIRD REJECTION");
    for(unsigned i=0;i<3;++i)
        check(std::memcmp(&clips[i],&original[i].position,sizeof clips[i])==0,"ASYNC DMA USED REUSED CALLER SCRATCH");
    check(unit.submit_prepare_world(geometry,submission)==Vu1WorldSubmitResult::submitted,"RELEASED BANK NOT REUSABLE");
    observe_async_submission(unit,submission);
    check(unit.cancel_world(b,clips)==Vu1WorldResult::rejected,"CANCEL ACCEPTED DURING PENDING WORK");
    check(unit.finish_prepare_world(submission,c,clips)==Vu1WorldPrepareResult::ready,"REUSED BANK FINISH");
    check(unit.cancel_world(b,clips)==Vu1WorldResult::fallback,"OTHER READY BANK LOST");
    check(unit.cancel_world(c,clips)==Vu1WorldResult::fallback,"REUSED BANK LOST");
    check(unit.submit_prepare_world(geometry,submission)==Vu1WorldSubmitResult::submitted,"DRAIN PREPARE");
    observe_async_submission(unit,submission);
    check(unit.drain_world(submission),"PREPARE DRAIN FAILED");
    check(unit.finish_prepare_world(submission,a,clips)==Vu1WorldPrepareResult::rejected,"DRAINED SUBMISSION ACCEPTED");
    check(!unit.drain_world(submission),"DRAINED SUBMISSION REUSED");
    return checks;
}
void material_memo_checks(Display& display,ps2::Vu1Transform& unit,std::uint64_t& words,unsigned& rejections,std::uint64_t& combined){
    auto source=std::make_unique<MemoMaterials>();PreparedWorldModel model;
    WorldScene normal,planner,limited;WorldSceneError error;WorldView view{identity()};
    normal.set_numeric_mode(WorldNumericMode::native_float);
    require(normal.configure(MemoMaterials::count,MemoMaterials::count*8,error),world_scene_error_message(error));
    require(planner.configure_plan(MemoMaterials::count,MemoMaterials::count*8,error),world_scene_error_message(error));
    require(limited.configure_plan(MemoMaterials::count,16,error),world_scene_error_message(error));
    PreparedWorldInstance instance{&model,identity()};unsigned collision_cases=0,mutations=0;
    std::uint64_t memo_hits=0,state_hits=0,epoch_invalidations=0,previous=0;
    for(unsigned phase=0;phase<3;++phase){
        // Publish a NEW immutable snapshot between calls. Caller-owned source
        // descriptors/palettes change at the same addresses; no live plan is
        // mutated, and prepared assets receive their new lifetime generations.
        display.invalidate_textures();
        if(phase==1){source->textures[0].width=16;++source->textures[0].generation;
            source->pixels0.fill(0x65);++mutations;}
        if(phase==2){for(auto& material:source->materials){material.palette[5]^=0x00ffff00u;
            material.palette[6]^=0x00ff00ffu;}++mutations;}
        require(model.assign(source->model(),error),world_scene_error_message(error));
        require(normal.update_prepared({&instance,1},view,error),world_scene_error_message(error));
        require(planner.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
        require(limited.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
        std::array<const Material*,64> buckets{};unsigned collisions=0;
        for(const auto& m:planner.draw_plan().instances()[0].materials){
            const auto p=reinterpret_cast<std::uintptr_t>(&m);auto& bucket=buckets[((p>>4)^(p>>10))&63];
            collisions+=bucket&&bucket!=&m;bucket=&m;
        }
        require(collisions>0,"MEMO COLLISION COVERAGE");++collision_cases;
        unsigned intra_batch_collisions=0;
        const auto& planned=planner.draw_plan().instances()[0];
        for(std::size_t first=192;first<MemoMaterials::count;first+=16){
            const auto* a=&planned.materials[planned.faces[first].material_index];
            const auto* b=&planned.materials[planned.faces[first+1].material_index];
            const auto ah=reinterpret_cast<std::uintptr_t>(a),bh=reinterpret_cast<std::uintptr_t>(b);
            intra_batch_collisions+=a!=b&&(((ah>>4)^(ah>>10))&63)==(((bh>>4)^(bh>>10))&63);
        }
        require(intra_batch_collisions>0,"MEMO INTRA-BATCH COLLISION NOT EXERCISED");
        require(display.present(normal.frame()),display.error());
        const auto expected=framebuffer(display,false,30+phase,words);
        require(!phase||expected!=previous,"MEMO MUTATION DID NOT CHANGE PIXELS");previous=expected;
        for(auto mode:{WorldSubmissionMode::fused,WorldSubmissionMode::classify_first,WorldSubmissionMode::classify_async}){
            // Warm resident control and memo must issue the same resource
            // touches/counters. Cold invalidation is a separately labeled case.
            require(display.present_world(planner.draw_plan(),unit,mode),display.error());
            const auto control=display.stats();
            require(!control.world_material_memo_hits&&!control.world_material_state_hits&&
                    !control.world_material_memo_misses,"CONTROL USED MATERIAL MEMO");
            require(framebuffer(display,true,30+phase,words)==expected,"MEMO CONTROL PIXELS");
            require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized),display.error());
            const auto memo=display.stats();
            require(material_counts(control)==material_counts(memo),"MEMO CHANGED WARM TOUCH/RESOURCE COUNTS");
            require(memo.world_material_memo_hits>0&&memo.world_material_state_hits>0&&
                    memo.world_material_residency_fast==MemoMaterials::count,"MEMO REUSE COVERAGE");
            require(memo.world_material_memo_hits+memo.world_material_memo_misses==MemoMaterials::count,
                    "MEMO REQUEST ACCOUNTING");
            require(framebuffer(display,true,30+phase,words)==expected,"MEMO WARM PIXELS");
            memo_hits+=memo.world_material_memo_hits;state_hits+=memo.world_material_state_hits;
        }
        display.invalidate_textures();
        require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first,WorldMaterialMode::memoized),display.error());
        // Two authored images plus the existing whole-frame checker image.
        require(display.stats().texture_uploads==3&&display.stats().world_material_memo_invalidations>0,
                "MEMO COLD UPLOAD INVALIDATION NOT EXERCISED");
        epoch_invalidations+=display.stats().world_material_memo_invalidations;
        require(framebuffer(display,true,30+phase,words)==expected,"MEMO COLD PIXELS");
        const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
        for(auto mode:{WorldSubmissionMode::fused,WorldSubmissionMode::classify_first,WorldSubmissionMode::classify_async}){
            require(!display.present_world(limited.draw_plan(),unit,mode,WorldMaterialMode::memoized),"MEMO LATE CAPACITY ACCEPTED");++rejections;
            require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"MEMO REJECTION NOT DRAINED");
            require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"MEMO REJECTION FLIPPED");
        }
        require(framebuffer(display,true,30+phase,words)==expected,"MEMO REJECTION CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_async,WorldMaterialMode::memoized),display.error());
        require(framebuffer(display,true,30+phase,words)==expected,"MEMO REJECTION RECOVERY");
        combined=(combined^expected)*1099511628211ull;
        std::printf("WORLD MATERIAL MEMO PHASE PASS phase=%u collisions=%u intra_batch=%u hits=%lu state_hits=%lu invalidations=%llu hash=%016llx\n",phase,
            collisions,intra_batch_collisions,(unsigned long)display.stats().world_material_memo_hits,(unsigned long)display.stats().world_material_state_hits,
            (unsigned long long)epoch_invalidations,(unsigned long long)expected);
    }
    const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
    require(!display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first,static_cast<WorldMaterialMode>(255)),
            "INVALID MATERIAL MODE ACCEPTED");++rejections;
    require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"INVALID MATERIAL MODE FLIPPED");
    std::printf("WORLD MATERIAL MEMO PASS phases=3 modes=3 collisions=%u mutations=%u warm_hits=%llu state_hits=%llu cold_invalidations=%llu late_rejections=9\n",
        collision_cases,mutations,(unsigned long long)memo_hits,(unsigned long long)state_hits,(unsigned long long)epoch_invalidations);
}
void trivial_reject_checks(Display& display,ps2::Vu1Transform& unit,Authored& source,OutsideTextures& outside,
                           std::uint64_t& words,unsigned& rejections,std::uint64_t& combined){
    WorldSceneError error=WorldSceneError::none;PreparedWorldModel model,outside_model,inside,huge;
    require(model.assign(source.model(),error),world_scene_error_message(error));
    require(outside_model.assign(outside.model(),error),world_scene_error_message(error));
    require(inside.assign({std::span(source.vertices).first(48),std::span(source.faces).first(16),{},source.materials},error),world_scene_error_message(error));
    const std::array<WorldPoint,3> huge_vertices{{{65536,0,.5f},{65536,.5f,.5f},{65536,0,.75f}}};
    WorldFace huge_face;huge_face.indices={0,1,2};
    require(huge.assign({huge_vertices,{&huge_face,1}},error),world_scene_error_message(error));
    WorldScene planner,limited;require(planner.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
    require(limited.configure_plan(face_count,16,error),world_scene_error_message(error));
    WorldView view{identity()};view.clear_color=0xff203040u;
    // The retained outside case has15 common-plane outside textured faces.
    // The other has crossing faces, a zero-area face, and a fully inside batch;
    // it must keep the public clipper and must report no trivial rejections.
    for(unsigned test=0;test<2;++test){
        PreparedWorldInstance instance{test?&model:&outside_model,identity()};
        require(planner.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
        for(auto mode:{WorldSubmissionMode::fused,WorldSubmissionMode::classify_first,WorldSubmissionMode::classify_async}){
            require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized),display.error()); // Warm resident control.
            const auto before=unit.world_stats();
            require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::clip_each),display.error());
            const auto control=display.stats();const auto middle=unit.world_stats();
            require(!control.world_trivial_rejects&&!control.texture_uploads&&!control.palette_uploads,"TRIVIAL CONTROL NOT WARM");
            const auto expected=framebuffer(display,false,40+test,words);
            require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::reject_outside),display.error());
            const auto candidate=display.stats();const auto after=unit.world_stats();
            require(fallback_control_counts(control)==fallback_control_counts(candidate),"TRIVIAL ORIGINAL COUNTERS CHANGED");
            require(candidate.world_trivial_rejects==(test?0u:15u),"TRIVIAL REJECTION COUNT");
            require(control.world_fallback_batches&&(!test||control.world_drawn_batches),"TRIVIAL PATH NOT EXERCISED");
            require(middle.batches-before.batches==after.batches-middle.batches&&
                    middle.corners-before.corners==after.corners-middle.corners&&
                    middle.dma_qwords-before.dma_qwords==after.dma_qwords-middle.dma_qwords,"TRIVIAL VU TRANSPORT CHANGED");
            require(framebuffer(display,true,40+test,words)==expected,"TRIVIAL PIXELS CHANGED");
            combined=(combined^expected)*1099511628211ull;
            std::printf("WORLD TRIVIAL REJECT PAIR PASS case=%u mode=%u rejected=%lu original_counters=34 driver_equal=1 hash=%016llx\n",
                test,unsigned(mode),(unsigned long)candidate.world_trivial_rejects,(unsigned long long)expected);
        }
    }
    std::array<PreparedWorldInstance,2> pair{{{&inside,identity()},{&inside,identity()}}};
    require(planner.plan_prepared(pair,view,error),world_scene_error_message(error));
    require(limited.plan_prepared(pair,view,error),world_scene_error_message(error));
    require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first,WorldMaterialMode::memoized),display.error());
    const auto expected=framebuffer(display,false,42,words);
    for(auto mode:{WorldSubmissionMode::fused,WorldSubmissionMode::classify_first,WorldSubmissionMode::classify_async}){
        const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
        require(!display.present_world(limited.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::reject_outside),"TRIVIAL LATE CAPACITY ACCEPTED");++rejections;
        require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1&&!display.stats().world_trivial_rejects,"TRIVIAL CAPACITY DID NOT DRAIN");
        require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"TRIVIAL CAPACITY FLIPPED");
        require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::reject_outside),display.error());
        require(framebuffer(display,true,42,words)==expected,"TRIVIAL CAPACITY RECOVERY");
        auto bad=pair;bad[1].model=&huge;bad[1].object_to_world[0]=65536;
        WorldScene numerical;require(numerical.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
        require(numerical.plan_prepared(bad,view,error),world_scene_error_message(error));
        const auto numeric_front1=*GS_REG_DISPFB1,numeric_front2=*GS_REG_DISPFB2;
        require(!display.present_world(numerical.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::reject_outside),"TRIVIAL SCALAR FAILURE HIDDEN");++rejections;
        require(display.stats().world_rejected_batches==1&&!display.stats().world_trivial_rejects&&
                display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"TRIVIAL SCALAR FAILURE NOT PRESERVED");
        require(*GS_REG_DISPFB1==numeric_front1&&*GS_REG_DISPFB2==numeric_front2,"TRIVIAL SCALAR FAILURE FLIPPED");
        require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized,WorldFallbackMode::reject_outside),display.error());
        require(framebuffer(display,true,42,words)==expected,"TRIVIAL SCALAR FAILURE RECOVERY");
        std::printf("WORLD TRIVIAL REJECT RECOVERY PASS mode=%u capacity=1 numerical=1 rejected_gate=0\n",unsigned(mode));
    }
    const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
    require(!display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first,WorldMaterialMode::memoized,
        static_cast<WorldFallbackMode>(255)),"INVALID FALLBACK MODE ACCEPTED");++rejections;
    require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"INVALID FALLBACK MODE FLIPPED");
    std::puts("WORLD TRIVIAL REJECT PASS pairs=6 positive=3 zero=3 original_counters=34 late_rejections=6 invalid_mode=1");
}
}
int main(){
    std::setvbuf(stdout,nullptr,_IONBF,0);std::setvbuf(stderr,nullptr,_IONBF,0);
    try {
        std::puts("WORLD DISPLAY BEGIN");
        *DMA_REG_CTRL|=1u;asm volatile("sync.l" ::: "memory");
        Display display;require(display.initialize(),display.error());
        std::puts("WORLD DISPLAY INITIALIZED");
        display.set_caption(nullptr);display.set_debug_page(nullptr);
        ps2::Vu1Transform unit;require(unit.initialize(),unit.error());
        std::puts("WORLD DISPLAY VU INITIALIZED");
        require(unit.initialize_world(),unit.error());
        std::puts("WORLD DISPLAY WORLD KERNEL INITIALIZED");
        std::puts("WORLD DISPLAY SOURCE CONSTRUCT");
        Authored source;PreparedWorldModel model;WorldSceneError error=WorldSceneError::none;
        std::puts("WORLD DISPLAY MODEL ASSIGN");
        require(model.assign(source.model(),error),world_scene_error_message(error));
        std::puts("WORLD DISPLAY MODEL ASSIGNED");
        WorldScene normal,planner,limited;
        normal.set_numeric_mode(WorldNumericMode::native_float);
        std::puts("WORLD DISPLAY NORMAL CONFIGURE");
        require(normal.configure(face_count,face_count*8,error),world_scene_error_message(error));
        std::puts("WORLD DISPLAY PLAN CONFIGURE");
        require(planner.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
        std::puts("WORLD DISPLAY LIMITED CONFIGURE");
        require(limited.configure_plan(face_count,1,error),world_scene_error_message(error));
        std::puts("WORLD DISPLAY CONFIGURED");
        PreparedWorldInstance instance{&model,identity()};WorldView view{identity()};
        std::uint64_t words=0,combined=1469598103934665603ull,previous=0;
        unsigned drawn=0,fallback=0,rejections=0;
        for(unsigned phase=0;phase<4;++phase){
            view.clear_color=0xff102030u+phase*0x070b11u;
            instance.object_to_world[12]=(phase&1)?.0625f:0;
            instance.object_to_world[13]=(phase&2)?-.0625f:0;
            display.set_vu0_packing((phase&1)!=0);
            std::printf("WORLD DISPLAY NORMAL UPDATE phase=%u\n",phase);
            require(normal.update_prepared({&instance,1},view,error),world_scene_error_message(error));
            std::printf("WORLD DISPLAY PLAN UPDATE phase=%u\n",phase);
            require(planner.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
            const auto triangles=normal.frame().triangles.size();
            std::printf("WORLD DISPLAY BASELINE PRESENT phase=%u triangles=%u\n",phase,unsigned(triangles));
            require(display.present(normal.frame()),display.error());
            std::printf("WORLD DISPLAY BASELINE PRESENTED phase=%u\n",phase);
            const auto expected=framebuffer(display,false,phase,words);
            require(!phase||expected!=previous,"VACUOUS UNCHANGED FRAMEBUFFER");previous=expected;
            std::printf("WORLD DISPLAY BASELINE phase=%u triangles=%u hash=%016llx\n",phase,unsigned(triangles),(unsigned long long)expected);
            require(display.present_world(planner.draw_plan(),unit),display.error());
            const auto stats=display.stats();
            require(stats.triangles==triangles&&stats.world_source_triangles==face_count,"EMITTED TRIANGLE COUNT");
            require(stats.world_batches==3&&stats.world_drawn_batches&&stats.world_fallback_batches,"DIRECT/FALLBACK PATH NOT EXERCISED");
            const auto actual=framebuffer(display,true,phase,words);require(actual==expected,"FRAMEBUFFER HASH");
            combined=(combined^actual)*1099511628211ull;drawn+=stats.world_drawn_batches;fallback+=stats.world_fallback_batches;
            std::printf("WORLD DISPLAY PAIR PASS phase=%u triangles=%u direct=%lu fallback=%lu handoffs=%lu empty_skips=%lu hash=%016llx\n",phase,unsigned(triangles),(unsigned long)stats.world_drawn_batches,(unsigned long)stats.world_fallback_batches,(unsigned long)stats.world_path3_handoffs,(unsigned long)stats.world_path3_empty_skips,(unsigned long long)actual);
            // Rejection must not publish the partially changed backbuffer.
            // The one-triangle cap fails after appending one valid triangle.
            const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
            require(!display.present_world(WorldDrawPlan{},unit),"EMPTY PLAN ACCEPTED");++rejections;
            require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"INVALID PLAN FLIPPED");
            require(limited.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
            require(!display.present_world(limited.draw_plan(),unit),"EXPANDED CAPACITY ACCEPTED");++rejections;
            require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"CAPACITY FAILURE FLIPPED");
            // Next successful submission clears and replaces all partial work.
            require(display.present_world(planner.draw_plan(),unit),display.error());
            require(framebuffer(display,true,phase,words)==expected,"RECOVERY FRAMEBUFFER");
        }
        // Two wholly inside instances require consecutive direct batches.
        // The second batch skips a redundant PATH3 FINISH; final presentation
        // must retire both batches' deferred GS work with one FINISH.
        PreparedWorldModel inside;
        require(inside.assign({std::span(source.vertices).first(48),std::span(source.faces).first(16),{},source.materials},error),world_scene_error_message(error));
        std::array<PreparedWorldInstance,2> consecutive{{{&inside,identity()},{&inside,identity()}}};
        consecutive[1].object_to_world[12]=.03125f;view.clear_color=0xff305010u;
        require(normal.update_prepared(consecutive,view,error),world_scene_error_message(error));
        require(planner.plan_prepared(consecutive,view,error),world_scene_error_message(error));
        require(display.present(normal.frame()),display.error());
        const auto expected=framebuffer(display,false,4,words);
        require(display.present_world(planner.draw_plan(),unit),display.error());
        const auto consecutive_stats=display.stats();
        require(consecutive_stats.world_drawn_batches==2&&!consecutive_stats.world_fallback_batches&&consecutive_stats.triangles==32,"CONSECUTIVE DIRECT BATCHES");
        require(consecutive_stats.world_path3_handoffs==1&&consecutive_stats.world_path3_empty_skips>=1&&consecutive_stats.world_gs_finishes==1&&consecutive_stats.world_path3_stream_retirements==1,"EOP HANDOFF/FINAL FINISH NOT EXERCISED");
        require(framebuffer(display,true,4,words)==expected,"CONSECUTIVE DIRECT FRAMEBUFFER");
        combined=(combined^expected)*1099511628211ull;drawn+=consecutive_stats.world_drawn_batches;
        std::printf("WORLD DISPLAY HANDOFF PASS direct=%lu handoffs=%lu empty_skips=%lu gs_finishes=%lu stream_retirements=%lu hash=%016llx\n",(unsigned long)consecutive_stats.world_drawn_batches,(unsigned long)consecutive_stats.world_path3_handoffs,(unsigned long)consecutive_stats.world_path3_empty_skips,(unsigned long)consecutive_stats.world_gs_finishes,(unsigned long)consecutive_stats.world_path3_stream_retirements,(unsigned long long)expected);
        WorldScene late;
        require(late.configure_plan(face_count,16,error),world_scene_error_message(error));
        require(late.plan_prepared(consecutive,view,error),world_scene_error_message(error));
        const auto front1=*GS_REG_DISPFB1,front2=*GS_REG_DISPFB2;
        require(!display.present_world(late.draw_plan(),unit),"LATE CAPACITY ACCEPTED");++rejections;
        require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"LATE CAPACITY DID NOT DRAIN DIRECT WORK");
        require(*GS_REG_DISPFB1==front1&&*GS_REG_DISPFB2==front2,"LATE CAPACITY FLIPPED");
        require(framebuffer(display,true,5,words)==expected,"LATE CAPACITY CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit),display.error());
        require(framebuffer(display,true,5,words)==expected,"LATE CAPACITY RECOVERY");
        // An admitted matrix can still produce out-of-domain clip values.
        // Failure after the first direct instance must drain its GS work too.
        const std::array<WorldPoint,3> huge_vertices{{{65536,0,.5f},{65536,.5f,.5f},{65536,0,.75f}}};
        WorldFace huge_face;huge_face.indices={0,1,2};PreparedWorldModel huge;
        require(huge.assign({huge_vertices,{&huge_face,1}},error),world_scene_error_message(error));
        auto bad=consecutive;bad[1].model=&huge;bad[1].object_to_world=identity();bad[1].object_to_world[0]=65536;
        require(late.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
        require(late.plan_prepared(bad,view,error),world_scene_error_message(error));
        const auto numerical_front1=*GS_REG_DISPFB1,numerical_front2=*GS_REG_DISPFB2;
        require(!display.present_world(late.draw_plan(),unit),"LATE NUMERICAL FAILURE ACCEPTED");++rejections;
        require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"LATE NUMERICAL FAILURE DID NOT DRAIN DIRECT WORK");
        require(*GS_REG_DISPFB1==numerical_front1&&*GS_REG_DISPFB2==numerical_front2,"LATE NUMERICAL FAILURE FLIPPED");
        require(framebuffer(display,true,6,words)==expected,"LATE NUMERICAL FAILURE CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit),display.error());
        require(framebuffer(display,true,6,words)==expected,"LATE NUMERICAL RECOVERY");
        std::puts("WORLD DISPLAY DEFERRED REJECTION PASS capacity=1 numerical=1 drained=2 recovered=2");
        // Exercise both directions with resident materials: PATH1 direct,
        // queued PATH3 clipped fans, then PATH1 direct again. The queued fans
        // must be delimited by EOP before the final direct batch, while one
        // GS FINISH still owns completion of the whole frame.
        require(normal.configure(face_count+16,(face_count+16)*8,error),world_scene_error_message(error));
        require(planner.configure_plan(face_count+16,(face_count+16)*8,error),world_scene_error_message(error));
        const std::array<PreparedWorldInstance,2> fallback_then_direct{{{&model,identity()},{&inside,identity()}}};
        view.clear_color=0xff203050u;
        require(normal.update_prepared(fallback_then_direct,view,error),world_scene_error_message(error));
        require(planner.plan_prepared(fallback_then_direct,view,error),world_scene_error_message(error));
        require(display.present(normal.frame()),display.error());
        const auto stream_expected=framebuffer(display,false,7,words);
        require(display.present_world(planner.draw_plan(),unit),display.error());
        const auto stream_stats=display.stats();
        std::printf("WORLD DISPLAY STREAM COUNTERS direct=%lu fallback=%lu stream_retirements=%lu gs_finishes=%lu\n",(unsigned long)stream_stats.world_drawn_batches,(unsigned long)stream_stats.world_fallback_batches,(unsigned long)stream_stats.world_path3_stream_retirements,(unsigned long)stream_stats.world_gs_finishes);
        // Each fallback batch queues fans that retire before the following
        // VU attempt, even when that next attempt also falls back: clear +2.
        require(stream_stats.world_drawn_batches==2&&stream_stats.world_fallback_batches==2&&stream_stats.world_path3_stream_retirements==3&&stream_stats.world_gs_finishes==1,"FALLBACK EOP HANDOFF NOT EXERCISED");
        require(framebuffer(display,true,7,words)==stream_expected,"FALLBACK EOP FRAMEBUFFER");
        combined=(combined^stream_expected)*1099511628211ull;drawn+=stream_stats.world_drawn_batches;fallback+=stream_stats.world_fallback_batches;
        std::printf("WORLD DISPLAY STREAM PASS direct=%lu fallback=%lu stream_retirements=%lu gs_finishes=%lu hash=%016llx\n",(unsigned long)stream_stats.world_drawn_batches,(unsigned long)stream_stats.world_fallback_batches,(unsigned long)stream_stats.world_path3_stream_retirements,(unsigned long)stream_stats.world_gs_finishes,(unsigned long long)stream_expected);
        // 32 four-page textures exceed the96-page arena;288 distinct CLUTs
        // exceed its256 slots. Consecutive direct batches therefore force both
        // resource upload paths after deferred draws, while all cells remain
        // visible for the final literal framebuffer comparison.
        auto eviction=std::make_unique<Eviction>();PreparedWorldModel eviction_model;
        require(eviction_model.assign(eviction->model(),error),world_scene_error_message(error));
        require(normal.configure(Eviction::count,Eviction::count*8,error),world_scene_error_message(error));
        require(planner.configure_plan(Eviction::count,Eviction::count*8,error),world_scene_error_message(error));
        PreparedWorldInstance eviction_instance{&eviction_model,identity()};view.clear_color=0xff302010u;
        require(normal.update_prepared({&eviction_instance,1},view,error),world_scene_error_message(error));
        require(planner.plan_prepared({&eviction_instance,1},view,error),world_scene_error_message(error));
        display.invalidate_textures();require(display.present(normal.frame()),display.error());
        const auto eviction_expected=framebuffer(display,false,8,words);
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit),display.error());
        const auto eviction_stats=display.stats();
        require(eviction_stats.world_drawn_batches==18&&!eviction_stats.world_fallback_batches&&eviction_stats.triangles==Eviction::count,"EVICTION DIRECT BATCHES");
        require(eviction_stats.texture_evictions>0&&eviction_stats.palette_uploads==Eviction::count&&eviction_stats.world_gs_finishes==18,"DEFERRED RESOURCE RETIREMENT NOT EXERCISED");
        require(framebuffer(display,true,8,words)==eviction_expected,"DEFERRED RESOURCE FRAMEBUFFER");
        combined=(combined^eviction_expected)*1099511628211ull;drawn+=eviction_stats.world_drawn_batches;
        std::printf("WORLD DISPLAY EVICTION PASS direct=%lu texture_evictions=%lu clut_uploads=%lu gs_finishes=%lu hash=%016llx\n",(unsigned long)eviction_stats.world_drawn_batches,(unsigned long)eviction_stats.texture_evictions,(unsigned long)eviction_stats.palette_uploads,(unsigned long)eviction_stats.world_gs_finishes,(unsigned long long)eviction_expected);
        // The same immutable plans compare Frame, fused world and geometry-first
        // world. Cache counters may differ; emitted pixels/order may not.
        for(unsigned phase=0;phase<4;++phase){
            view.clear_color=0xff102030u+phase*0x070b11u;
            instance.object_to_world=identity();instance.object_to_world[12]=(phase&1)?.0625f:0;
            instance.object_to_world[13]=(phase&2)?-.0625f:0;
            display.set_vu0_packing((phase&1)!=0);
            require(normal.update_prepared({&instance,1},view,error),world_scene_error_message(error));
            require(planner.plan_prepared({&instance,1},view,error),world_scene_error_message(error));
            require(display.present(normal.frame()),display.error());
            const auto triplet_expected=framebuffer(display,false,10+phase,words);
            require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::fused),display.error());
            const auto fused=display.stats();
            require(framebuffer(display,true,10+phase,words)==triplet_expected,"FUSED TRIPLET PIXELS");
            require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first),display.error());
            const auto classified=display.stats();
            require(classified.triangles==fused.triangles&&classified.world_drawn_batches==fused.world_drawn_batches&&
                    classified.world_fallback_batches==fused.world_fallback_batches,"CLASSIFIED PATH COUNTS");
            require(classified.world_prepared_batches==3&&classified.world_ready_batches==classified.world_drawn_batches&&
                    classified.world_emit_calls==classified.world_drawn_batches,"CLASSIFIED PREPARE/EMIT COUNTS");
            require(framebuffer(display,true,10+phase,words)==triplet_expected,"CLASSIFIED TRIPLET PIXELS");
            require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_async),display.error());
            const auto asynchronous=display.stats();
            require(asynchronous.triangles==classified.triangles&&asynchronous.checker_triangles==classified.checker_triangles&&
                    asynchronous.world_source_triangles==classified.world_source_triangles&&
                    asynchronous.world_drawn_batches==classified.world_drawn_batches&&
                    asynchronous.world_fallback_batches==classified.world_fallback_batches&&
                    asynchronous.world_skipped_preload_materials==classified.world_skipped_preload_materials,
                    "ASYNC ORDERED COUNTERS");
            require(asynchronous.world_async_prepare_submissions==3&&asynchronous.world_async_emit_submissions==1&&
                    asynchronous.world_async_gathered_triangles==18,"ASYNC BOTH GATHER INTERVALS");
            require(framebuffer(display,true,10+phase,words)==triplet_expected,"ASYNC QUADRUPLET PIXELS");
            std::printf("WORLD DISPLAY ASYNC PAIR PASS phase=%u submitted_prepare=%lu submitted_emit=%lu gathered=%lu hash=%016llx\n",phase,
                (unsigned long)asynchronous.world_async_prepare_submissions,(unsigned long)asynchronous.world_async_emit_submissions,
                (unsigned long)asynchronous.world_async_gathered_triangles,(unsigned long long)triplet_expected);
            combined=(combined^triplet_expected)*1099511628211ull;
            std::printf("WORLD DISPLAY TRIPLET PASS phase=%u prepared=%lu ready=%lu direct=%lu fallback=%lu skipped_preloads=%lu hash=%016llx\n",phase,
                (unsigned long)classified.world_prepared_batches,(unsigned long)classified.world_ready_batches,
                (unsigned long)classified.world_drawn_batches,(unsigned long)classified.world_fallback_batches,
                (unsigned long)classified.world_skipped_preload_materials,(unsigned long long)triplet_expected);
        }
        // Geometry rejection must happen before cold texture residency. The
        // first flat face survives;15 unrelated textured faces lie wholly offscreen.
        OutsideTextures outside_source;PreparedWorldModel outside_model;
        require(outside_model.assign(outside_source.model(),error),world_scene_error_message(error));
        PreparedWorldInstance outside_instance{&outside_model,identity()};view.clear_color=0xff203040u;
        require(normal.update_prepared({&outside_instance,1},view,error),world_scene_error_message(error));
        require(planner.plan_prepared({&outside_instance,1},view,error),world_scene_error_message(error));
        require(normal.frame().triangles.size()==1,"OUTSIDE FIXTURE VISIBILITY");
        display.invalidate_textures();require(display.present(normal.frame()),display.error());
        const auto outside_expected=framebuffer(display,false,14,words);
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::fused),display.error());
        const auto outside_fused=display.stats();
        require(outside_fused.texture_uploads==15&&outside_fused.world_fallback_batches==1,"OUTSIDE FUSED PRELOADS");
        require(framebuffer(display,true,14,words)==outside_expected,"OUTSIDE FUSED PIXELS");
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first),display.error());
        const auto outside_classified=display.stats();
        require(outside_classified.triangles==1&&outside_classified.texture_uploads==0&&outside_classified.texture_hits==0&&
                outside_classified.palette_uploads==0&&outside_classified.palette_hits==0,"OUTSIDE CLASSIFIED TOUCHED MATERIALS");
        require(outside_classified.world_prepared_batches==1&&outside_classified.world_ready_batches==0&&
                outside_classified.world_fallback_batches==1&&outside_classified.world_skipped_preload_materials==15,"OUTSIDE CLASSIFIED COUNTS");
        require(framebuffer(display,true,14,words)==outside_expected,"OUTSIDE CLASSIFIED PIXELS");
        std::printf("WORLD DISPLAY OUTSIDE PASS fused_uploads=%lu classified_uploads=%lu skipped_preloads=%lu hash=%016llx\n",
            (unsigned long)outside_fused.texture_uploads,(unsigned long)outside_classified.texture_uploads,
            (unsigned long)outside_classified.world_skipped_preload_materials,(unsigned long long)outside_expected);
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_async),display.error());
        require(display.stats().world_async_prepare_submissions==1&&!display.stats().world_async_emit_submissions&&
                !display.stats().world_async_gathered_triangles&&!display.stats().texture_uploads&&!display.stats().texture_hits&&
                !display.stats().palette_uploads&&!display.stats().palette_hits,"ASYNC OUTSIDE TOUCHED RESOURCES");
        require(framebuffer(display,true,14,words)==outside_expected,"ASYNC OUTSIDE PIXELS");
        const auto token_front1=*GS_REG_DISPFB1,token_front2=*GS_REG_DISPFB2;
        const auto tokens=token_checks(unit,display.output_height());
        const auto async_tokens=async_token_checks(unit,display.output_height());
        require(*GS_REG_DISPFB1==token_front1&&*GS_REG_DISPFB2==token_front2,"TOKEN TEST FLIPPED");
        require(framebuffer(display,true,15,words)==outside_expected,"TOKEN TEST EMITTED GEOMETRY");
        require(!display.present_world(planner.draw_plan(),unit,static_cast<WorldSubmissionMode>(255)),"INVALID WORLD MODE ACCEPTED");++rejections;
        require(*GS_REG_DISPFB1==token_front1&&*GS_REG_DISPFB2==token_front2,"INVALID MODE FLIPPED");
        std::printf("WORLD DISPLAY TOKEN PASS checks=%u no_emission=1\n",tokens);
        std::printf("WORLD DISPLAY ASYNC TOKEN PASS checks=%u two_banks=1 busy_recovery=1 owned_staging=1 no_emission=1\n",async_tokens);
        // Capacity rejection of a second ready batch retrieves its retained
        // clips; it does not silently recompute, emit, or drop the first batch.
        require(normal.update_prepared(consecutive,view,error),world_scene_error_message(error));
        require(planner.plan_prepared(consecutive,view,error),world_scene_error_message(error));
        require(display.present(normal.frame()),display.error());
        const auto ready_expected=framebuffer(display,false,16,words);
        require(late.configure_plan(face_count,16,error),world_scene_error_message(error));
        require(late.plan_prepared(consecutive,view,error),world_scene_error_message(error));
        const auto ready_front1=*GS_REG_DISPFB1,ready_front2=*GS_REG_DISPFB2;
        require(!display.present_world(late.draw_plan(),unit,WorldSubmissionMode::classify_first),"CLASSIFIED LATE CAPACITY ACCEPTED");++rejections;
        const auto cancelled=display.stats();
        require(cancelled.world_prepared_batches==2&&cancelled.world_ready_batches==2&&cancelled.world_emit_calls==1&&
                cancelled.world_drawn_batches==1&&cancelled.world_gs_finishes==1,"CLASSIFIED READY CANCEL/DRAIN");
        require(*GS_REG_DISPFB1==ready_front1&&*GS_REG_DISPFB2==ready_front2,"CLASSIFIED CAPACITY FLIPPED");
        require(framebuffer(display,true,16,words)==ready_expected,"CLASSIFIED CAPACITY CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first),display.error());
        require(framebuffer(display,true,16,words)==ready_expected,"CLASSIFIED CAPACITY RECOVERY");
        require(late.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
        require(late.plan_prepared(bad,view,error),world_scene_error_message(error));
        const auto failed_front1=*GS_REG_DISPFB1,failed_front2=*GS_REG_DISPFB2;
        require(!display.present_world(late.draw_plan(),unit,WorldSubmissionMode::classify_first),"CLASSIFIED NUMERICAL FAILURE ACCEPTED");++rejections;
        require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"CLASSIFIED NUMERICAL DRAIN");
        require(*GS_REG_DISPFB1==failed_front1&&*GS_REG_DISPFB2==failed_front2,"CLASSIFIED NUMERICAL FLIPPED");
        require(framebuffer(display,true,17,words)==ready_expected,"CLASSIFIED NUMERICAL CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first),display.error());
        require(framebuffer(display,true,17,words)==ready_expected,"CLASSIFIED NUMERICAL RECOVERY");
        std::puts("WORLD DISPLAY CLASSIFIED REJECTION PASS capacity=1 numerical=1 drained=2 recovered=2");
        const auto async_front1=*GS_REG_DISPFB1,async_front2=*GS_REG_DISPFB2;
        require(late.configure_plan(face_count,16,error),world_scene_error_message(error));
        require(late.plan_prepared(consecutive,view,error),world_scene_error_message(error));
        require(!display.present_world(late.draw_plan(),unit,WorldSubmissionMode::classify_async),"ASYNC LATE CAPACITY ACCEPTED");++rejections;
        require(display.stats().world_async_prepare_submissions==2&&display.stats().world_async_emit_submissions==1&&
                display.stats().world_gs_finishes==1,"ASYNC CAPACITY DRAIN");
        require(*GS_REG_DISPFB1==async_front1&&*GS_REG_DISPFB2==async_front2,"ASYNC CAPACITY FLIPPED");
        require(framebuffer(display,true,17,words)==ready_expected,"ASYNC CAPACITY CHANGED FRONT");
        require(late.configure_plan(face_count,face_count*8,error),world_scene_error_message(error));
        require(late.plan_prepared(bad,view,error),world_scene_error_message(error));
        require(!display.present_world(late.draw_plan(),unit,WorldSubmissionMode::classify_async),"ASYNC NUMERICAL FAILURE ACCEPTED");++rejections;
        require(display.stats().world_drawn_batches==1&&display.stats().world_gs_finishes==1,"ASYNC NUMERICAL DRAIN");
        require(*GS_REG_DISPFB1==async_front1&&*GS_REG_DISPFB2==async_front2,"ASYNC NUMERICAL FLIPPED");
        require(framebuffer(display,true,17,words)==ready_expected,"ASYNC NUMERICAL CHANGED FRONT");
        require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_async),display.error());
        require(framebuffer(display,true,17,words)==ready_expected,"ASYNC REJECTION RECOVERY");
        require(display.stats().world_async_gathered_triangles==16,"ASYNC CROSS INSTANCE LOOKAHEAD");
        std::puts("WORLD DISPLAY ASYNC REJECTION PASS capacity=1 numerical=1 drained=2 recovered=1 cross_instance=1");
        // Actual uploads between ready preparation and emission must preserve
        // retained geometry and complete preceding GS work before eviction.
        require(normal.update_prepared({&eviction_instance,1},view,error),world_scene_error_message(error));
        require(planner.plan_prepared({&eviction_instance,1},view,error),world_scene_error_message(error));
        display.invalidate_textures();require(display.present(normal.frame()),display.error());
        const auto classified_eviction_expected=framebuffer(display,false,18,words);
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_first),display.error());
        const auto classified_eviction=display.stats();
        require(classified_eviction.world_prepared_batches==18&&classified_eviction.world_ready_batches==18&&
                classified_eviction.world_emit_calls==18&&classified_eviction.world_drawn_batches==18&&
                classified_eviction.texture_evictions>0&&classified_eviction.palette_uploads==Eviction::count,
                "CLASSIFIED RETAINED GEOMETRY/RESOURCE EVICTION");
        require(framebuffer(display,true,18,words)==classified_eviction_expected,"CLASSIFIED EVICTION PIXELS");
        combined=(combined^classified_eviction_expected)*1099511628211ull;
        std::printf("WORLD DISPLAY CLASSIFIED EVICTION PASS ready=%lu direct=%lu texture_evictions=%lu clut_uploads=%lu hash=%016llx\n",
            (unsigned long)classified_eviction.world_ready_batches,(unsigned long)classified_eviction.world_drawn_batches,
            (unsigned long)classified_eviction.texture_evictions,(unsigned long)classified_eviction.palette_uploads,
            (unsigned long long)classified_eviction_expected);
        display.invalidate_textures();require(display.present_world(planner.draw_plan(),unit,WorldSubmissionMode::classify_async),display.error());
        const auto async_eviction=display.stats();
        require(async_eviction.world_async_prepare_submissions==18&&async_eviction.world_async_emit_submissions==18&&
                async_eviction.world_async_gathered_triangles==272&&async_eviction.texture_evictions>0&&
                async_eviction.palette_uploads==Eviction::count,"ASYNC EVICTION/LOOKAHEAD COVERAGE");
        require(framebuffer(display,true,18,words)==classified_eviction_expected,"ASYNC EVICTION PIXELS");
        std::printf("WORLD DISPLAY ASYNC EVICTION PASS prepared=%lu emit=%lu gathered=%lu texture_evictions=%lu clut_uploads=%lu\n",
            (unsigned long)async_eviction.world_async_prepare_submissions,(unsigned long)async_eviction.world_async_emit_submissions,
            (unsigned long)async_eviction.world_async_gathered_triangles,(unsigned long)async_eviction.texture_evictions,
            (unsigned long)async_eviction.palette_uploads);
        for(auto mode:{WorldSubmissionMode::fused,WorldSubmissionMode::classify_first,WorldSubmissionMode::classify_async}){
            display.invalidate_textures();
            require(display.present_world(planner.draw_plan(),unit,mode,WorldMaterialMode::memoized),display.error());
            const auto memo=display.stats();
            require(memo.world_drawn_batches==18&&memo.texture_evictions>0&&memo.palette_uploads==Eviction::count&&
                    memo.world_material_memo_misses==Eviction::count,"MEMO EVICTION COVERAGE");
            require(framebuffer(display,true,18,words)==classified_eviction_expected,"MEMO EVICTION PIXELS");
            std::printf("WORLD MATERIAL MEMO EVICTION PASS mode=%u evictions=%lu palette_uploads=%lu misses=%lu\n",unsigned(mode),
                (unsigned long)memo.texture_evictions,(unsigned long)memo.palette_uploads,(unsigned long)memo.world_material_memo_misses);
        }
        // Exercise the separate full-FINISH packet tail. The fixture owns the
        // idle GS exclusively: after an empty Frame it draws into the currently
        // bound framebuffer without changing DISPFB. Full completion precedes
        // readback and every subsequent Display operation.
        std::array<Vertex,3> finished_vertices{{{124,288,0,0,1,.5f},{372,288,0,0,1,.5f},{248,96,0,0,1,.5f}}};
        Material finished_material;finished_material.color=0xff2040c0u;
        Triangle finished_triangle{finished_vertices,&finished_material};
        const Frame finished_frame{{&finished_triangle,1},0xff112233u,false};
        require(display.present(finished_frame),display.error());
        const auto finished_expected=framebuffer(display,false,19,words);
        require(display.present({{},finished_frame.clear_color,false}),display.error());
        require(display.readback_rect(320,display.output_height()/2,8,4,std::span(stripe).first(32))==ReadbackStatus::ok,
                "FULL FINISH CLEAR TARGET READBACK");
        require(stripe[0]!=reference[(display.output_height()/2)*640+320],"FULL FINISH TARGET STILL BASELINE");
        const auto finished_front1=*GS_REG_DISPFB1,finished_front2=*GS_REG_DISPFB2;
        const std::array<ps2::Vu1WorldCorner,3> finished_corners{{{{-.5f,-.5f,.5f,1},0,0,1,1},
                                                               {{.5f,-.5f,.5f,1},0,0,1,1},
                                                               {{0,.5f,.5f,1},0,0,1,1}}};
        const ps2::Vu1WorldGeometry finished_geometry{identity(),identity(),finished_corners,display.output_height(),900};
        std::array<ps2::TransformVector4,3> finished_clips{};ps2::Vu1WorldToken finished_token;
        require(unit.prepare_world(finished_geometry,finished_token,finished_clips)==ps2::Vu1WorldPrepareResult::ready,"FULL FINISH PREPARE");
        ps2::Vu1WorldState finished_state;
        finished_state.test=GS_SET_TEST(1,1,0x80,0,0,1,1,1);
        finished_state.prim=3;finished_state.rgba=0x80c04020u;
        const auto finished_copy=finished_token;
        require(unit.emit_world(finished_token,{&finished_state,1},ps2::Vu1WorldCompletion::gs_finished)==ps2::Vu1WorldResult::drawn,"FULL FINISH EMIT");
        require(unit.emit_world(finished_copy,{&finished_state,1},ps2::Vu1WorldCompletion::gs_finished)==ps2::Vu1WorldResult::rejected,"EMIT TOKEN REUSED");
        require(*GS_REG_DISPFB1==finished_front1&&*GS_REG_DISPFB2==finished_front2,"LOW LEVEL EMIT FLIPPED");
        require(framebuffer(display,true,19,words)==finished_expected,"FULL FINISH PIXELS");
        combined=(combined^finished_expected)*1099511628211ull;
        std::printf("WORLD DISPLAY FULL FINISH PASS emitted=1 consumed_copy_rejected=1 hash=%016llx\n",(unsigned long long)finished_expected);
        // Two prepared banks coexist; finishing one emit must not consume
        // the other bank. Both completion methods are exercised independently.
        for(unsigned drain=0;drain<2;++drain){
            require(display.present({{},finished_frame.clear_color,false}),display.error());
            const auto async_finished_front1=*GS_REG_DISPFB1,async_finished_front2=*GS_REG_DISPFB2;
            ps2::Vu1WorldSubmission pending;ps2::Vu1WorldToken emitting,other;
            require(unit.submit_prepare_world(finished_geometry,pending)==ps2::Vu1WorldSubmitResult::submitted,"ASYNC FULL FINISH PREPARE");
            observe_async_submission(unit,pending);
            require(unit.finish_prepare_world(pending,emitting,finished_clips)==ps2::Vu1WorldPrepareResult::ready,"ASYNC FULL FINISH READY");
            require(unit.submit_prepare_world(finished_geometry,pending)==ps2::Vu1WorldSubmitResult::submitted,"ASYNC OTHER BANK PREPARE");
            observe_async_submission(unit,pending);
            require(unit.finish_prepare_world(pending,other,finished_clips)==ps2::Vu1WorldPrepareResult::ready,"ASYNC OTHER BANK READY");
            require(unit.submit_emit_world(emitting,{&finished_state,1},ps2::Vu1WorldCompletion::gs_finished,pending)==ps2::Vu1WorldSubmitResult::submitted,"ASYNC FULL FINISH EMIT");
            observe_async_submission(unit,pending);
            const auto pending_copy=pending;
            ps2::Vu1WorldToken wrong_output;
            require(unit.finish_prepare_world(pending,wrong_output,finished_clips)==ps2::Vu1WorldPrepareResult::rejected,"ASYNC EMIT WRONG FINISH ACCEPTED");
            if(drain)require(unit.drain_world(pending),"ASYNC EMIT DRAIN FAILED");
            else require(unit.finish_emit_world(pending)==ps2::Vu1WorldResult::drawn,"ASYNC EMIT FINISH FAILED");
            require(unit.finish_emit_world(pending_copy)==ps2::Vu1WorldResult::rejected,"ASYNC EMIT SUBMISSION REUSED");
            require(unit.cancel_world(other,finished_clips)==ps2::Vu1WorldResult::fallback,"ASYNC EMIT CONSUMED OTHER BANK");
            require(*GS_REG_DISPFB1==async_finished_front1&&*GS_REG_DISPFB2==async_finished_front2,"ASYNC LOW LEVEL FLIPPED");
            require(framebuffer(display,true,20+drain,words)==finished_expected,"ASYNC FULL FINISH PIXELS");
        }
        std::puts("WORLD DISPLAY ASYNC FULL FINISH PASS finish=1 drain=1 other_bank_retained=2");
        material_memo_checks(display,unit,words,rejections,combined);
        trivial_reject_checks(display,unit,source,outside_source,words,rejections,combined);
        std::printf("WORLD DISPLAY ASYNC POLLS pending=%u already_complete=%u hardware_overlap_not_inferred=1\n",async_pending_polls,async_complete_polls);
        std::printf("WORLD DISPLAY PASS phases=4 consecutive_pair=1 fallback_pair=1 eviction_pair=1 classified_triplets=4 outside_triplet=1 full_finish_pair=1 token_checks=%u legacy_direct=%u legacy_fallback=%u no_flip_rejections=%u pixel_words=%llu hash=%016llx\n",tokens,drawn,fallback,rejections,(unsigned long long)words,(unsigned long long)combined);
    }catch(const std::exception& error){std::printf("WORLD DISPLAY FAIL: %s\n",error.what());}
    while(true)SleepThread();
}
