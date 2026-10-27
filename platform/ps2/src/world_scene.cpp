#include "world_scene.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>
#include <vector>

namespace ps2::native {
struct WorldScene::Storage {
    struct Bank {std::vector<Material> materials;std::vector<Triangle> triangles;};
    std::array<Bank,2> banks;
    std::size_t source_capacity=0,output_capacity=0,count=0;
    unsigned active=0;std::uint32_t clear=0xff101923u;WorldSceneStats stats{};
};
namespace {
bool value(float x){const auto m=std::bit_cast<std::uint32_t>(x)&0x7fffffffu;return !m||(m>=0x00800000u&&m<=0x47800000u);}
bool matrix(const WorldMatrix& m,bool affine){for(auto x:m)if(!value(x))return false;
    return !affine||(m[3]==0&&m[7]==0&&m[11]==0&&m[15]==1);}
bool fail(WorldSceneError e,WorldSceneError& output){output=e;return false;}
std::array<double,4> transform(const WorldMatrix& m,const std::array<double,4>& p){std::array<double,4> out{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)out[row]+=double(m[row+4*col])*p[col];return out;}
struct Interval {double low=0,high=0;};
double outward(double x,bool up){
    if(x==0)return std::bit_cast<double>(up?std::uint64_t{1}:std::uint64_t{0x8000000000000001ull});
    auto bits=std::bit_cast<std::uint64_t>(x);
    if((x>0)==up)++bits;else --bits;
    return std::bit_cast<double>(bits);
}
std::array<Interval,4> transform_bounds(const WorldMatrix& m,const std::array<Interval,4>& p){
    std::array<Interval,4> result{};
    // Preserve the scalar path's multiply/add order, widening each operation
    // by one binary64 ULP. Inputs are finite and bounded; no overflow is possible.
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
        const double coefficient=m[row+column*4];
        if(coefficient==0)continue;
        const auto a=coefficient>=0?p[column].low:p[column].high;
        const auto b=coefficient>=0?p[column].high:p[column].low;
        result[row].low=outward(result[row].low+outward(coefficient*a,false),false);
        result[row].high=outward(result[row].high+outward(coefficient*b,true),true);
    }
    return result;
}
bool outside_group(const WorldInstance& instance,const WorldFaceGroup& group,const WorldMatrix& view){
    std::array<float,3> low{},high{};bool first=true;
    for(std::size_t i=group.first_face;i<std::size_t(group.first_face)+group.face_count;++i){
        for(auto index:instance.model.faces[i].indices){const auto p=instance.model.vertices[index];
            const std::array<float,3> v{p.x,p.y,p.z};
            for(unsigned axis=0;axis<3;++axis){
                if(first)low[axis]=high[axis]=v[axis];
                else{low[axis]=std::min(low[axis],v[axis]);high[axis]=std::max(high[axis],v[axis]);}
            }
            first=false;
        }
    }
    // Every source component is already a validated binary32 normal or zero.
    // Extrema comparisons therefore need no software binary64 arithmetic; only
    // the six selected endpoints are widened, exactly, for interval transforms.
    std::array<Interval,4> bounds{};bounds[3]={1,1};
    for(unsigned axis=0;axis<3;++axis)bounds[axis]={low[axis],high[axis]};
    const auto clip=transform_bounds(view,transform_bounds(instance.object_to_world,bounds));
    std::array<Interval,4> rounded{};
    for(unsigned axis=0;axis<4;++axis){
        // Rejection must not hide an invalid transformed vertex. If the box
        // could cross the subnormal range or the transform limit, take the
        // ordinary per-face path, which reports the original error.
        if(clip[axis].low < -clip_component_limit||clip[axis].high > clip_component_limit)return false;
        const float low=float(clip[axis].low),high=float(clip[axis].high);
        const auto low_bits=std::bit_cast<std::uint32_t>(low),high_bits=std::bit_cast<std::uint32_t>(high);
        const auto low_magnitude=low_bits&0x7fffffffu,high_magnitude=high_bits&0x7fffffffu;
        // EE floating-point comparisons can treat subnormals as zero. Use
        // encodings for this proof, just as the downstream clip validator does.
        if(!((low_magnitude==0&&high_magnitude==0)||
             (!(low_bits>>31)&&low_magnitude>=0x00800000u&&!(high_bits>>31)&&high_magnitude>=low_magnitude)||
             ((high_bits>>31)&&high_magnitude>=0x00800000u&&(low_bits>>31)&&low_magnitude>=high_magnitude)))return false;
        rounded[axis]={low,high};
    }
    const auto x=rounded[0],y=rounded[1],z=rounded[2],w=rounded[3];
    return w.high<clip_min_w||x.high+w.high<0||w.high-x.low<0||
        y.high+w.high<0||w.high-y.low<0||z.high<0||w.high-z.low<0;
}
}
WorldScene::WorldScene()noexcept=default;WorldScene::~WorldScene()=default;
const char* world_scene_error_message(WorldSceneError e)noexcept{switch(e){
    case WorldSceneError::none:return "NONE";case WorldSceneError::not_configured:return "WORLD FRAME STORAGE NOT CONFIGURED";
    case WorldSceneError::invalid_capacity:return "INVALID WORLD FRAME CAPACITY";case WorldSceneError::invalid_model:return "INVALID WORLD MODEL";
    case WorldSceneError::invalid_transform:return "INVALID WORLD TRANSFORM";case WorldSceneError::input_limit:return "WORLD INPUT CAPACITY EXCEEDED";
    case WorldSceneError::output_limit:return "WORLD OUTPUT CAPACITY EXCEEDED";case WorldSceneError::clipping_failed:return "WORLD CLIPPING FAILED";
    default:return "WORLD FRAME ALLOCATION FAILED";}}
bool WorldScene::configure(std::size_t faces,std::size_t triangles,WorldSceneError& error)noexcept{
    if(!faces||faces>32768||!triangles||triangles>16384)return fail(WorldSceneError::invalid_capacity,error);
    try{auto replacement=std::make_unique<Storage>();replacement->source_capacity=faces;replacement->output_capacity=triangles;
        replacement->stats.persistent_bytes=sizeof(Storage);
        for(auto& bank:replacement->banks){bank.materials.resize(faces);bank.triangles.resize(triangles);
            replacement->stats.persistent_bytes+=bank.materials.capacity()*sizeof(Material)+bank.triangles.capacity()*sizeof(Triangle);}
        storage_.swap(replacement);error=WorldSceneError::none;return true;
    }catch(const std::bad_alloc&){return fail(WorldSceneError::allocation_failed,error);}
    catch(const std::length_error&){return fail(WorldSceneError::allocation_failed,error);}
}
bool WorldScene::update(std::span<const WorldInstance> instances,const WorldView& view,WorldSceneError& error)noexcept{
    if(!storage_)return fail(WorldSceneError::not_configured,error);
    if(instances.size()>1024)return fail(WorldSceneError::input_limit,error);
    if(!matrix(view.world_to_clip,false)||(view.clear_color>>24)!=255)return fail(WorldSceneError::invalid_transform,error);
    std::size_t vertices=0,faces=0;
    for(const auto& instance:instances){const auto& model=instance.model;
        if(!matrix(instance.object_to_world,true))return fail(WorldSceneError::invalid_transform,error);
        if(model.vertices.size()<3||model.faces.empty())return fail(WorldSceneError::invalid_model,error);
        if(model.vertices.size()>65536-vertices||model.faces.size()>storage_->source_capacity-faces)return fail(WorldSceneError::input_limit,error);
        vertices+=model.vertices.size();faces+=model.faces.size();
        for(auto p:model.vertices)if(!value(p.x)||!value(p.y)||!value(p.z))return fail(WorldSceneError::invalid_model,error);
        for(auto face:model.faces){if((face.argb>>24)!=255||face.indices[0]==face.indices[1]||face.indices[1]==face.indices[2]||face.indices[0]==face.indices[2])return fail(WorldSceneError::invalid_model,error);
            for(auto index:face.indices)if(index>=model.vertices.size())return fail(WorldSceneError::invalid_model,error);}
        if(!model.groups.empty()){
            std::size_t end=0;
            for(const auto group:model.groups){if(group.first_face!=end||!group.face_count||group.face_count>model.faces.size()-end)
                    return fail(WorldSceneError::invalid_model,error);
                end+=group.face_count;
            }
            if(end!=model.faces.size())return fail(WorldSceneError::invalid_model,error);
        }
    }
    auto& bank=storage_->banks[storage_->active^1];std::size_t material_index=0,triangle_count=0;
    std::uint32_t groups=0,culled_groups=0,transformed_vertices=0;
    for(const auto& instance:instances){std::size_t group_index=0;
    for(std::size_t face_index=0;face_index<instance.model.faces.size();++face_index){
        if(group_index<instance.model.groups.size()&&instance.model.groups[group_index].first_face==face_index){
            const auto group=instance.model.groups[group_index++];++groups;
            if(outside_group(instance,group,view.world_to_clip)){
                ++culled_groups;material_index+=group.face_count;face_index+=group.face_count-1;continue;
            }
        }
        const auto& face=instance.model.faces[face_index];
        auto& material=bank.materials[material_index++];material.color=face.argb;
        ClipTriangle input;input.material=&material;
        for(unsigned i=0;i<3;++i){const auto p=instance.model.vertices[face.indices[i]];
            ++transformed_vertices;
            const auto world=transform(instance.object_to_world,{p.x,p.y,p.z,1});
            const auto clip=transform(view.world_to_clip,world);
            for(double x:clip)if(!std::isfinite(x)||std::fabs(x)>clip_component_limit)return fail(WorldSceneError::invalid_transform,error);
            input.vertices[i]={float(clip[0]),float(clip[1]),float(clip[2]),float(clip[3]),0,0};
        }
        std::array<Triangle,clip_triangle_capacity> clipped;
        const auto result=clip_project_triangle(input,clipped);
        if(result.status!=ClipStatus::success)return fail(WorldSceneError::clipping_failed,error);
        if(result.count>storage_->output_capacity-triangle_count)return fail(WorldSceneError::output_limit,error);
        for(std::size_t i=0;i<result.count;++i)bank.triangles[triangle_count++]=clipped[i];
    }}
    storage_->active^=1;storage_->count=triangle_count;storage_->clear=view.clear_color;
    storage_->stats={std::uint32_t(instances.size()),std::uint32_t(vertices),std::uint32_t(faces),std::uint32_t(triangle_count),storage_->stats.persistent_bytes};
    storage_->stats.groups=groups;storage_->stats.culled_groups=culled_groups;storage_->stats.transformed_vertices=transformed_vertices;
    error=WorldSceneError::none;return true;
}
Frame WorldScene::frame()const noexcept{return storage_?Frame{{storage_->banks[storage_->active].triangles.data(),storage_->count},storage_->clear,true}:Frame{};}
WorldSceneStats WorldScene::stats()const noexcept{return storage_?storage_->stats:WorldSceneStats{};}
} // namespace ps2::native
