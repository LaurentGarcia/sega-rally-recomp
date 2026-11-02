// Original authored geometry: complete Frame/PATH3 versus world/PATH1 pixels.
// No game assets, instruction runtime or captured transformed vertices.
#include "native_display.h"
#include "world_scene.h"
#include "vu1_transform.h"
#include <dma_registers.h>
#include <gs_privileged.h>
#include <kernel.h>
#include <algorithm>
#include <array>
#include <cstdio>
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
        std::printf("WORLD DISPLAY PASS phases=4 consecutive_pair=1 fallback_pair=1 eviction_pair=1 direct=%u fallback=%u no_flip_rejections=%u pixel_words=%llu hash=%016llx\n",drawn,fallback,rejections,(unsigned long long)words,(unsigned long long)combined);
    }catch(const std::exception& error){std::printf("WORLD DISPLAY FAIL: %s\n",error.what());}
    while(true)SleepThread();
}
