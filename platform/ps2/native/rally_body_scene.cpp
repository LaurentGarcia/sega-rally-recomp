#include "rally_body_scene.h"
#include "contact_point_preparation.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace ps2::native {
namespace {
constexpr std::size_t header_bytes=64, vertex_cap=65536, face_cap=16384;
constexpr std::size_t file_cap=header_bytes+vertex_cap*12+face_cap*16;
std::uint32_t word(std::span<const std::uint8_t> data,std::size_t p) noexcept {
    return data[p]|(std::uint32_t(data[p+1])<<8)|(std::uint32_t(data[p+2])<<16)|(std::uint32_t(data[p+3])<<24);
}
float scalar(std::span<const std::uint8_t> data,std::size_t p) noexcept {return std::bit_cast<float>(word(data,p));}
bool valid(float x) noexcept {
    const auto magnitude=std::bit_cast<std::uint32_t>(x)&0x7fffffffu;
    return magnitude==0||(magnitude>=0x00800000u&&magnitude<=0x47800000u);
}
bool affine(const WorldMatrix& m) noexcept {
    for(auto x:m)if(!valid(x))return false;
    return m[3]==0&&m[7]==0&&m[11]==0&&m[15]==1;
}
std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc=~0u;
    for(auto byte:bytes){crc^=byte;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
    return ~crc;
}
bool failure(RallyBodyError& error,RallyBodyError value) noexcept {error=value;return false;}
}
struct RallyBodyScene::Storage {
    std::vector<WorldPoint> vertices;
    std::vector<WorldFace> faces;
    std::vector<WorldPoint> course_vertices;
    std::vector<WorldFace> course_faces;
    std::vector<WorldFaceGroup> course_groups;
    WorldMatrix bridge{};
    std::unique_ptr<WorldScene> scene;
};
RallyBodyScene::RallyBodyScene() noexcept=default;
RallyBodyScene::~RallyBodyScene()=default;
void RallyBodyScene::set_profile_clock(WorldSceneClock clock,void* context) noexcept {
    clock_=clock;clock_context_=context;
    if(storage_)storage_->scene->set_profile_clock(clock,context);
}
void RallyBodyScene::set_transform_provider(WorldSceneTransformBatch batch,void* context) noexcept {
    transform_batch_=batch;transform_context_=context;
    if(storage_)storage_->scene->set_transform_provider(batch,context);
}
void RallyBodyScene::set_numeric_mode(WorldNumericMode mode) noexcept {
    numeric_mode_=mode;
    if(storage_)storage_->scene->set_numeric_mode(mode);
}

bool RallyBodyScene::decode(std::span<const std::uint8_t> bytes,const WorldMatrix& bridge,
                            RallyBodyError& error) noexcept {
    if(!affine(bridge))return failure(error,RallyBodyError::invalid_bridge);
    if(bytes.size()<header_bytes||bytes.size()>file_cap||std::memcmp(bytes.data(),"RLYBODY\0",8)!=0)
        return failure(error,RallyBodyError::invalid_header);
    const auto vertices=word(bytes,16),faces=word(bytes,20);
    if(word(bytes,8)!=1||word(bytes,12)!=header_bytes||word(bytes,32)!=12||word(bytes,36)!=16)
        return failure(error,RallyBodyError::invalid_header);
    for(unsigned i=40;i<header_bytes;++i)if(bytes[i])return failure(error,RallyBodyError::invalid_header);
    if(vertices<3||vertices>vertex_cap||faces==0||faces>face_cap)return failure(error,RallyBodyError::capacity);
    const auto payload=std::size_t(vertices)*12+std::size_t(faces)*16;
    if(word(bytes,24)!=payload||bytes.size()!=header_bytes+payload||crc32(bytes.subspan(header_bytes))!=word(bytes,28))
        return failure(error,RallyBodyError::invalid_header);
    try {
        auto next=std::make_unique<Storage>();next->bridge=bridge;
        next->vertices.resize(vertices);next->faces.resize(faces);
        for(unsigned i=0;i<vertices;++i){const auto offset=header_bytes+i*12;
            const WorldPoint p{scalar(bytes,offset),scalar(bytes,offset+4),scalar(bytes,offset+8)};
            if(!valid(p.x)||!valid(p.y)||!valid(p.z))return failure(error,RallyBodyError::invalid_geometry);
            next->vertices[i]=p;
        }
        for(unsigned i=0;i<faces;++i){const auto offset=header_bytes+vertices*12+i*16;
            const WorldFace f{{word(bytes,offset),word(bytes,offset+4),word(bytes,offset+8)},word(bytes,offset+12)};
            if(f.indices[0]>=vertices||f.indices[1]>=vertices||f.indices[2]>=vertices||
               f.indices[0]==f.indices[1]||f.indices[0]==f.indices[2]||f.indices[1]==f.indices[2]||
               (f.argb>>24)!=255)return failure(error,RallyBodyError::invalid_geometry);
            next->faces[i]=f;
        }
        WorldSceneError engine_error;next->scene=std::make_unique<WorldScene>();
        // Clipping may expand a triangle. Reserve at most the display limit,
        // and reject an overflowing frame rather than dropping geometry.
        if(!next->scene->configure(faces,std::min<std::size_t>(16384,std::size_t(faces)*8),engine_error))
            return failure(error,RallyBodyError::allocation_failed);
        next->scene->set_profile_clock(clock_,clock_context_);
        next->scene->set_transform_provider(transform_batch_,transform_context_);
        next->scene->set_numeric_mode(numeric_mode_);
        storage_.swap(next);error=RallyBodyError::none;return true;
    }catch(const std::bad_alloc&){return failure(error,RallyBodyError::allocation_failed);}
     catch(const std::length_error&){return failure(error,RallyBodyError::capacity);}
}
bool RallyBodyScene::load(const char* path,const WorldMatrix& bridge,RallyBodyError& error) noexcept {
    if(!path||!*path)return failure(error,RallyBodyError::invalid_file);
    auto* file=std::fopen(path,"rb");if(!file)return failure(error,RallyBodyError::invalid_file);
    try {
        std::array<std::uint8_t,header_bytes> header{};
        if(std::fread(header.data(),1,header.size(),file)!=header.size()){
            std::fclose(file);return failure(error,RallyBodyError::invalid_file);
        }
        const auto payload=word(header,24);
        if(payload>file_cap-header_bytes){std::fclose(file);return failure(error,RallyBodyError::capacity);}
        std::vector<std::uint8_t> bytes(header_bytes+payload);
        std::memcpy(bytes.data(),header.data(),header_bytes);
        const bool read=std::fread(bytes.data()+header_bytes,1,payload,file)==payload;
        const auto tail=std::fgetc(file);const bool io_error=std::ferror(file)!=0;std::fclose(file);file=nullptr;
        if(!read||tail!=EOF||io_error)return failure(error,RallyBodyError::invalid_file);
        return decode(bytes,bridge,error);
    }catch(const std::bad_alloc&){if(file)std::fclose(file);return failure(error,RallyBodyError::allocation_failed);}
     catch(const std::length_error&){if(file)std::fclose(file);return failure(error,RallyBodyError::capacity);}
}
bool RallyBodyScene::attach_course(std::span<const ContactSegment> course,RallyBodyError& error) noexcept {
    if(!storage_)return failure(error,RallyBodyError::not_loaded);
    if(course.size()>65535)return failure(error,RallyBodyError::capacity);
    std::size_t vertex_count=0,face_count=0;
    for(const auto& segment:course)for(const auto& polygon:segment.polygons){
        // Bit 2 selects four active corners; a triangle's fourth corner is
        // unused payload and must neither be drawn nor validated.
        const unsigned corners=(polygon.attributes&4u)?4u:3u;
        if(corners>65536-storage_->vertices.size()-vertex_count||
           corners-2>32768-storage_->faces.size()-face_count)
            return failure(error,RallyBodyError::capacity);
        vertex_count+=corners;face_count+=corners-2;
    }
    try {
        std::vector<WorldPoint> vertices(vertex_count);
        std::vector<WorldFace> faces(face_count);
        std::vector<WorldFaceGroup> groups;groups.reserve(std::min(course.size(),face_count));
        std::size_t vertex_index=0,face_index=0;unsigned segment_index=0;
        for(const auto& segment:course){
            const auto color=(segment_index++&1)?0xff526354u:0xff425346u;
            const auto first_face=face_index;
            for(const auto& polygon:segment.polygons){
                const unsigned corners=(polygon.attributes&4u)?4u:3u;
                const auto first=std::uint32_t(vertex_index);
                for(unsigned k=0;k<corners;++k){const auto p=polygon.vertices[k];
                    if(!valid(p.x)||!valid(p.y)||!valid(p.z))return failure(error,RallyBodyError::invalid_geometry);
                    vertices[vertex_index++]={p.x,p.y,p.z};
                }
                faces[face_index++]={{first,first+1,first+2},color};
                if(corners==4)faces[face_index++]={{first,first+2,first+3},color};
            }
            if(face_index!=first_face)groups.push_back({std::uint32_t(first_face),std::uint32_t(face_index-first_face)});
        }
        auto scene=std::make_unique<WorldScene>();WorldSceneError engine_error;
        const auto total_faces=storage_->faces.size()+faces.size();
        if(!scene->configure(total_faces,std::min<std::size_t>(16384,total_faces*8),engine_error))
            return failure(error,RallyBodyError::allocation_failed);
        scene->set_profile_clock(clock_,clock_context_);
        scene->set_transform_provider(transform_batch_,transform_context_);
        scene->set_numeric_mode(numeric_mode_);
        storage_->course_vertices.swap(vertices);storage_->course_faces.swap(faces);
        storage_->course_groups.swap(groups);
        storage_->scene.swap(scene);error=RallyBodyError::none;return true;
    }catch(const std::bad_alloc&){return failure(error,RallyBodyError::allocation_failed);}
     catch(const std::length_error&){return failure(error,RallyBodyError::capacity);}
}
bool RallyBodyScene::update(const RallyBodyPose& pose,const WorldView& view,RallyBodyError& error) noexcept {
    if(!storage_)return failure(error,RallyBodyError::not_loaded);
    ContactPointPreparation prepared;
    if(!prepare_contact_points(pose.orientation,pose.position,{},prepared))return failure(error,RallyBodyError::invalid_pose);
    WorldMatrix car{};
    for(unsigned i=0;i<3;++i){const auto p=prepared.matrix.columns[i];car[4*i]=p.x;car[4*i+1]=p.y;car[4*i+2]=p.z;}
    car[12]=pose.position.x;car[13]=pose.position.y;car[14]=pose.position.z;car[15]=1;
    WorldMatrix combined{};
    for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row){
        if(numeric_mode_==WorldNumericMode::native_float){
            float value=0;
            for(unsigned k=0;k<4;++k)value+=car[k*4+row]*storage_->bridge[column*4+k];
            combined[column*4+row]=value;continue;
        }
        double value=0;
        for(unsigned k=0;k<4;++k)value+=double(car[k*4+row])*storage_->bridge[column*4+k];
        combined[column*4+row]=float(value);
    }
    if(!affine(combined))return failure(error,RallyBodyError::invalid_pose);
    const WorldMatrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<WorldInstance,2> instances{};std::size_t count=0;
    if(!storage_->course_faces.empty())instances[count++]={{storage_->course_vertices,storage_->course_faces,storage_->course_groups},identity};
    instances[count++]={{storage_->vertices,storage_->faces},combined};
    WorldSceneError engine_error;
    if(!storage_->scene->update(std::span(instances.data(),count),view,engine_error))return failure(error,RallyBodyError::presentation_failed);
    error=RallyBodyError::none;return true;
}
Frame RallyBodyScene::frame() const noexcept {return storage_?storage_->scene->frame():Frame{};}
WorldSceneStats RallyBodyScene::stats() const noexcept {return storage_?storage_->scene->stats():WorldSceneStats{};}
std::size_t RallyBodyScene::persistent_bytes() const noexcept {
    return storage_?sizeof(Storage)+sizeof(WorldScene)+
        (storage_->vertices.capacity()+storage_->course_vertices.capacity())*sizeof(WorldPoint)+
        (storage_->faces.capacity()+storage_->course_faces.capacity())*sizeof(WorldFace)+
        storage_->course_groups.capacity()*sizeof(WorldFaceGroup)+
        storage_->scene->stats().persistent_bytes:0;
}
const char* rally_body_error_message(RallyBodyError error) noexcept {
    switch(error){
    case RallyBodyError::none:return "NONE";
    case RallyBodyError::invalid_file:return "BODY FILE READ FAILED";
    case RallyBodyError::invalid_header:return "BODY HEADER OR CHECKSUM INVALID";
    case RallyBodyError::invalid_geometry:return "BODY GEOMETRY INVALID";
    case RallyBodyError::invalid_bridge:return "BODY MODEL BRIDGE INVALID";
    case RallyBodyError::invalid_pose:return "BODY POSE OUTSIDE DOMAIN";
    case RallyBodyError::capacity:return "BODY CAPACITY EXCEEDED";
    case RallyBodyError::allocation_failed:return "BODY ALLOCATION FAILED";
    case RallyBodyError::presentation_failed:return "BODY FRAME REJECTED";
    case RallyBodyError::not_loaded:return "BODY NOT LOADED";
    }
    return "UNKNOWN BODY ERROR";
}
} // namespace ps2::native
