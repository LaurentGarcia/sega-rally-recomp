#include "rally_visual_scene.h"
#include "native_textured_model.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace ps2::native {
namespace {
constexpr std::size_t config_bytes=320;
constexpr std::size_t model_file_limit=64+65536*12+32768*68+4096*76+1024*16+4*1024*1024;
std::uint32_t word(const std::uint8_t* p) noexcept {
    return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;
}
float number(const std::uint8_t* p) noexcept{return std::bit_cast<float>(word(p));}
std::uint32_t crc_update(std::uint32_t crc,std::span<const std::uint8_t> bytes) noexcept {
    for(auto b:bytes){crc^=b;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
    return crc;
}
bool fail(RallyVisualError& error,RallyVisualError code) noexcept{error=code;return false;}
WorldMatrix matrix(const AffineTransform3& source) noexcept {
    WorldMatrix result{};
    for(unsigned i=0;i<3;++i){const auto p=source.columns[i];result[i*4]=p.x;result[i*4+1]=p.y;result[i*4+2]=p.z;}
    result[12]=source.translation.x;result[13]=source.translation.y;result[14]=source.translation.z;result[15]=1;
    return result;
}
bool valid_config(const RallyVisualConfig& c) noexcept {
    if(!c.asset_count||c.asset_count>7||(c.clear_color>>24)!=255)return false;
    std::array<bool,7> used{};
    for(auto role:c.role_assets){if(role>=c.asset_count)return false;used[role]=true;}
    for(unsigned i=0;i<c.asset_count;++i)
        if(!used[i]||c.assets[i].bytes<64||c.assets[i].bytes>model_file_limit)return false;
    for(unsigned i=c.asset_count;i<7;++i)if(c.assets[i].bytes||c.assets[i].crc32)return false;
    const auto near_bits=std::bit_cast<std::uint32_t>(c.near_z),far_bits=std::bit_cast<std::uint32_t>(c.far_z);
    if(near_bits<0x3a800000u||near_bits>0x47800000u||far_bits<0x3a800000u||far_bits>0x47800000u||
       !(c.near_z<c.far_z))return false;
    const float range=c.far_z-c.near_z,scale=c.far_z/range,offset=c.near_z*scale;
    if(!(scale<=65536&&offset<=65536))return false;
    auto state=c.initial_view_state;DrivingRenderView view;CarRenderBridge bridge;
    return update_driving_render_view(c.initial_pose,c.view,state,view)&&
        build_car_render_bridge(c.initial_pose,c.geometry,view.world_to_view,bridge);
}
bool path(char (&output)[512],const char* directory,const char* leaf) noexcept {
    if(!directory||!*directory)return false;
    const auto n=std::strlen(directory);if(n>450)return false;
    const int count=std::snprintf(output,sizeof output,"%s%s%s",directory,directory[n-1]=='/'?"":"/",leaf);
    return count>0&&std::size_t(count)<sizeof output;
}
bool read_asset(const char* filename,RallyVisualAssetIdentity expected,std::vector<std::uint8_t>& bytes) {
    std::unique_ptr<FILE,decltype(&std::fclose)> file(std::fopen(filename,"rb"),&std::fclose);
    if(!file)return false;
    bytes.resize(expected.bytes);
    const bool read=std::fread(bytes.data(),1,bytes.size(),file.get())==bytes.size();
    const auto extra=std::fgetc(file.get());
    const bool okay=read&&extra==EOF&&!std::ferror(file.get())&&~crc_update(~0u,bytes)==expected.crc32;
    const bool closed=std::fclose(file.release())==0;return okay&&closed;
}
bool prepare_model(WorldModel source,PreparedWorldModel& output,WorldSceneError& error) {
    // Preserve input order while enabling bounded group rejection for asset
    // files without a precomputed partition. Bounds remain engine-owned.
    std::vector<WorldFaceGroup> groups;
    if(source.groups.empty()){
        groups.reserve((source.faces.size()+31)/32);
        for(std::size_t first=0;first<source.faces.size();first+=32)
            groups.push_back({std::uint32_t(first),std::uint32_t(std::min<std::size_t>(32,source.faces.size()-first))});
        source.groups=groups;
    }
    return output.assign(source,error);
}
}
struct RallyVisualScene::Storage {
    RallyVisualConfig config{};
    DrivingRenderViewState state{};
    std::array<PreparedWorldModel,7> models;
    std::array<std::size_t,7> vertices{},faces{};
    WorldScene scene;
    bool first=true,published=false;
    bool initialize_scene(WorldSceneError& error) noexcept {
        std::size_t vertex_count=0,face_count=0;
        for(auto role:config.role_assets){
            if(vertices[role]>65536-vertex_count||faces[role]>32768-face_count){error=WorldSceneError::input_limit;return false;}
            vertex_count+=vertices[role];face_count+=faces[role];
        }
        state=config.initial_view_state;
        return scene.configure(face_count,std::min<std::size_t>(16384,face_count*8),error);
    }
};
RallyVisualScene::RallyVisualScene() noexcept=default;
RallyVisualScene::~RallyVisualScene()=default;
bool decode_rally_visual_config(std::span<const std::uint8_t> data,std::uint32_t expected,
                               RallyVisualConfig& output,RallyVisualError& error) noexcept {
    if(data.size()!=config_bytes||std::memcmp(data.data(),"RLYVIEW1",8)||word(data.data()+8)!=1||
       word(data.data()+12)!=config_bytes||word(data.data()+16)!=config_bytes-32||word(data.data()+28)||
       ~crc_update(~0u,data.subspan(32))!=word(data.data()+20))return fail(error,RallyVisualError::config);
    RallyVisualConfig next;next.initial_game_frame=word(data.data()+24);
    if(next.initial_game_frame!=expected)return fail(error,RallyVisualError::frame_mismatch);
    std::size_t cursor=32;
    const auto words=[&](auto& target){for(auto& v:target){v=word(data.data()+cursor);cursor+=4;}};
    words(next.initial_pose.words.position);words(next.initial_pose.words.motion);
    words(next.initial_pose.words.orientation);words(next.initial_pose.words.attributes);
    next.initial_pose.flags=word(data.data()+112);next.initial_pose.progress=word(data.data()+116);
    next.initial_view_state.yaw_history=number(data.data()+120);next.view.distance_adjustment=number(data.data()+124);
    for(unsigned i=0;i<4;++i){const auto* p=data.data()+128+i*12;next.geometry.wheel_offsets[i]={number(p),number(p+4),number(p+8)};}
    next.near_z=number(data.data()+176);next.far_z=number(data.data()+180);
    next.clear_color=word(data.data()+184);next.asset_count=word(data.data()+188);
    for(unsigned i=0;i<7;++i){next.role_assets[i]=word(data.data()+192+i*4);
        next.assets[i]={word(data.data()+220+i*8),word(data.data()+224+i*8)};}
    for(unsigned i=276;i<config_bytes;++i)if(data[i])return fail(error,RallyVisualError::config);
    if(!valid_config(next))return fail(error,RallyVisualError::config);
    output=next;error=RallyVisualError::none;return true;
}
bool RallyVisualScene::configure(const RallyVisualConfig& config,std::span<const WorldModel> models,
                                 RallyVisualError& error) noexcept {
    if(!valid_config(config)||models.size()!=config.asset_count)return fail(error,RallyVisualError::config);
    try {
        auto next=std::make_unique<Storage>();next->config=config;WorldSceneError engine_error;
        for(std::size_t i=0;i<models.size();++i){
            if(!prepare_model(models[i],next->models[i],engine_error))return fail(error,engine_error==WorldSceneError::allocation_failed?RallyVisualError::allocation:RallyVisualError::asset);
            next->vertices[i]=models[i].vertices.size();next->faces[i]=models[i].faces.size();
        }
        if(!next->initialize_scene(engine_error))return fail(error,engine_error==WorldSceneError::allocation_failed?RallyVisualError::allocation:RallyVisualError::capacity);
        next->scene.set_profile_clock(clock_,clock_context_);next->scene.set_transform_provider(provider_,provider_context_);next->scene.set_numeric_mode(mode_);
        storage_.swap(next);timings_={};error=RallyVisualError::none;return true;
    }catch(const std::bad_alloc&){return fail(error,RallyVisualError::allocation);}
     catch(const std::length_error&){return fail(error,RallyVisualError::capacity);}
}
bool RallyVisualScene::load(const char* directory,std::uint32_t expected,RallyVisualError& error) noexcept {
    char filename[512];if(!path(filename,directory,"view.bin"))return fail(error,RallyVisualError::io);
    FILE* file=std::fopen(filename,"rb");if(!file)return fail(error,RallyVisualError::io);
    std::array<std::uint8_t,config_bytes> bytes{};
    const bool read=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();
    const auto tail=std::fgetc(file);const bool okay=read&&tail==EOF&&!std::ferror(file);
    const bool closed=std::fclose(file)==0;if(!okay||!closed)return fail(error,RallyVisualError::io);
    RallyVisualConfig config;if(!decode_rally_visual_config(bytes,expected,config,error))return false;
    try {
        auto next=std::make_unique<Storage>();next->config=config;WorldSceneError engine_error;
        for(unsigned i=0;i<config.asset_count;++i){
            char leaf[16];std::snprintf(leaf,sizeof leaf,"model%u.bin",i);
            if(!path(filename,directory,leaf))return fail(error,RallyVisualError::io);
            std::vector<std::uint8_t> data;
            if(!read_asset(filename,config.assets[i],data))return fail(error,RallyVisualError::asset_identity);
            TexturedModel model;TexturedModelError model_error;
            if(!decode_textured_model(data,model,model_error))return fail(error,model_error==TexturedModelError::allocation?RallyVisualError::allocation:RallyVisualError::asset);
            const auto view=model.model();next->vertices[i]=view.vertices.size();next->faces[i]=view.faces.size();
            if(!prepare_model(view,next->models[i],engine_error))return fail(error,engine_error==WorldSceneError::allocation_failed?RallyVisualError::allocation:RallyVisualError::asset);
        }
        if(!next->initialize_scene(engine_error))return fail(error,engine_error==WorldSceneError::allocation_failed?RallyVisualError::allocation:RallyVisualError::capacity);
        next->scene.set_profile_clock(clock_,clock_context_);next->scene.set_transform_provider(provider_,provider_context_);next->scene.set_numeric_mode(mode_);
        storage_.swap(next);timings_={};error=RallyVisualError::none;return true;
    }catch(const std::bad_alloc&){return fail(error,RallyVisualError::allocation);}
     catch(const std::length_error&){return fail(error,RallyVisualError::capacity);}
}
bool RallyVisualScene::update(const RenderPose& pose,RallyVisualError& error) noexcept {
    if(!storage_)return fail(error,RallyVisualError::not_loaded);
    auto& s=*storage_;if(s.first&&pose!=s.config.initial_pose)return fail(error,RallyVisualError::initial_pose);
    const auto now=[&]() noexcept{return clock_?clock_(clock_context_):std::uint64_t{0};};
    const auto start=now();auto next_state=s.state;DrivingRenderView camera;CarRenderBridge bridge;
    if(!update_driving_render_view(pose,s.config.view,next_state,camera)||
       !build_car_render_bridge(pose,s.config.geometry,camera.world_to_view,bridge))return fail(error,RallyVisualError::pose);
    std::array<PreparedWorldInstance,7> instances{};
    instances[0]={&s.models[s.config.role_assets[0]],matrix(camera.world_to_view)};
    instances[1]={&s.models[s.config.role_assets[1]],matrix(bridge.shadow)};
    instances[2]={&s.models[s.config.role_assets[2]],matrix(bridge.body)};
    for(unsigned i=0;i<4;++i)instances[3+i]={&s.models[s.config.role_assets[3+i]],matrix(bridge.wheels[i])};
    WorldView view;view.clear_color=s.config.clear_color;view.use_facing=true;
    view.world_to_clip[0]=camera.focus_x/248.f;view.world_to_clip[5]=camera.focus_y/192.f;
    const float range=s.config.far_z-s.config.near_z;
    view.world_to_clip[10]=s.config.far_z/range;view.world_to_clip[11]=1;
    view.world_to_clip[14]=-s.config.near_z*view.world_to_clip[10];
    const auto scene_start=now();WorldSceneError engine_error;
    if(!s.scene.update_prepared(instances,view,engine_error))return fail(error,RallyVisualError::presentation);
    s.state=next_state;s.first=false;s.published=true;timings_={scene_start-start,now()-scene_start};
    error=RallyVisualError::none;return true;
}
void RallyVisualScene::reset() noexcept {
    if(storage_){storage_->state=storage_->config.initial_view_state;storage_->first=true;storage_->published=false;}
    timings_={};
}
void RallyVisualScene::set_profile_clock(WorldSceneClock clock,void* context) noexcept {
    clock_=clock;clock_context_=context;if(storage_)storage_->scene.set_profile_clock(clock,context);
}
void RallyVisualScene::set_transform_provider(WorldSceneTransformBatch provider,void* context) noexcept {
    provider_=provider;provider_context_=context;if(storage_)storage_->scene.set_transform_provider(provider,context);
}
void RallyVisualScene::set_numeric_mode(WorldNumericMode mode) noexcept {
    mode_=mode;if(storage_)storage_->scene.set_numeric_mode(mode);
}
Frame RallyVisualScene::frame()const noexcept{return storage_&&storage_->published?storage_->scene.frame():Frame{};}
WorldSceneStats RallyVisualScene::stats()const noexcept{return storage_&&storage_->published?storage_->scene.stats():WorldSceneStats{};}
RallyVisualTimings RallyVisualScene::timings()const noexcept{return timings_;}
std::size_t RallyVisualScene::persistent_bytes()const noexcept {
    if(!storage_)return 0;
    std::size_t bytes=sizeof(Storage)+storage_->scene.stats().persistent_bytes;
    for(const auto& model:storage_->models)bytes+=model.persistent_bytes();return bytes;
}
const char* rally_visual_error_message(RallyVisualError error) noexcept {
    switch(error){
    case RallyVisualError::none:return "NONE";
    case RallyVisualError::not_loaded:return "VISUAL ASSETS NOT LOADED";
    case RallyVisualError::io:return "VISUAL FILE READ FAILED";
    case RallyVisualError::config:return "VISUAL CONFIG INVALID";
    case RallyVisualError::frame_mismatch:return "VISUAL INITIAL FRAME MISMATCH";
    case RallyVisualError::asset_identity:return "VISUAL ASSET SIZE OR CRC MISMATCH";
    case RallyVisualError::asset:return "VISUAL MODEL INVALID";
    case RallyVisualError::capacity:return "VISUAL CAPACITY EXCEEDED";
    case RallyVisualError::allocation:return "VISUAL ALLOCATION FAILED";
    case RallyVisualError::initial_pose:return "VISUAL INITIAL POSE MISMATCH";
    case RallyVisualError::pose:return "VISUAL POSE OUTSIDE DOMAIN";
    case RallyVisualError::presentation:return "VISUAL FRAME REJECTED";
    }return "UNKNOWN VISUAL ERROR";
}
}
