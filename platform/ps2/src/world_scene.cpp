#include "world_scene.h"
#include "native_geometry_float.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>
#include <vector>

namespace ps2::native {
struct WorldScene::Storage {
    struct Bank {std::vector<Material> materials;std::vector<Triangle> triangles;};
    struct CachedVertex {ClipVertex clip;std::uint32_t index=0,serial=0;};
    struct Batch {
        std::array<ps2::TransformVector4,ps2::vu1_transform_capacity> input,output;
        std::array<std::uint32_t,ps2::vu1_transform_capacity> indices{};
        std::array<std::uint8_t,ps2::vu1_transform_capacity> accepted{};
        std::size_t count=0,end_face=0;
    } batch;
    std::array<Bank,2> banks;
    std::vector<CachedVertex> vertex_cache;std::uint32_t cache_serial=0;
    std::size_t source_capacity=0,output_capacity=0,count=0;
    unsigned active=0;std::uint32_t clear=0xff101923u;WorldSceneStats stats{};
};
namespace {
constexpr std::uint32_t cache_serial_mask=0x7fffffffu,approximate_vertex=0x80000000u;
bool value(float x){const auto m=std::bit_cast<std::uint32_t>(x)&0x7fffffffu;return !m||(m>=0x00800000u&&m<=0x47800000u);}
bool matrix(const WorldMatrix& m,bool affine){for(auto x:m)if(!value(x))return false;
    return !affine||(m[3]==0&&m[7]==0&&m[11]==0&&m[15]==1);}
bool valid_batch_result(const ps2::TransformVector4& v,bool outside=false){
    if(!value(v.x)||!value(v.y)||!value(v.z)||!value(v.w))return false;
    if(outside)return true;
    const float x=v.x,y=v.y,z=v.z,w=v.w;
    return w>=clip_min_w&&x>=-w&&x<=w&&y>=-w&&y<=w&&z>=0&&z<=w;
}
bool fail(WorldSceneError e,WorldSceneError& output){output=e;return false;}
std::array<double,4> transform(const WorldMatrix& m,const std::array<double,4>& p){std::array<double,4> out{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)out[row]+=double(m[row+4*col])*p[col];return out;}
std::array<float,4> transform_float(const WorldMatrix& m,const std::array<float,4>& p){std::array<float,4> out{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)out[row]+=m[row+4*col]*p[col];return out;}
bool scalar_vertex(const WorldInstance& instance,const WorldMatrix& view,std::uint32_t index,
                   bool native,ClipVertex& result){
    const auto p=instance.model.vertices[index];
    if(native){
        const auto world=transform_float(instance.object_to_world,{p.x,p.y,p.z,1});
        const auto clip=transform_float(view,world);
        for(float x:clip)if(!value(x))return false;
        result={clip[0],clip[1],clip[2],clip[3],0,0};
    }else{
        const auto world=transform(instance.object_to_world,{p.x,p.y,p.z,1});
        const auto clip=transform(view,world);
        for(double x:clip)if(!std::isfinite(x)||std::fabs(x)>clip_component_limit)return false;
        result={float(clip[0]),float(clip[1]),float(clip[2]),float(clip[3]),0,0};
    }
    return true;
}
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
struct FloatInterval {float low=0,high=0;};
// Eight encoding steps enclose four ulps even across an exponent boundary:
// interval evaluation rounding plus the provider's two-ulp allowance. A VU
// flush-to-zero result is enclosed too; endpoints are never subnormal.
float outward_float(float x,bool up){
    auto bits=std::bit_cast<std::uint32_t>(x);const auto magnitude=bits&0x7fffffffu;
    if(magnitude<0x00800000u)return up?0x1p-126f:-0x1p-126f;
    const bool negative=(bits>>31)!=0;
    if(up!=negative)bits+=8;else bits-=8;
    if((bits&0x7fffffffu)<0x00800000u)return up?0.f:-0.f;
    return std::bit_cast<float>(bits);
}
float multiply_bound(float a,float b,bool up){
    if(a==0||b==0)return a*b;
    if(a==1)return b;if(a==-1)return -b;
    return outward_float(a*b,up);
}
float add_bound(float a,float b,bool up){
    if(a==0)return b;if(b==0)return a;
    return outward_float(a+b,up);
}
std::array<FloatInterval,4> transform_bounds_float(const WorldMatrix& m,const std::array<FloatInterval,4>& p){
    std::array<FloatInterval,4> result{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
        const float coefficient=m[row+column*4];if(coefficient==0)continue;
        const auto a=coefficient>=0?p[column].low:p[column].high;
        const auto b=coefficient>=0?p[column].high:p[column].low;
        result[row].low=add_bound(result[row].low,multiply_bound(coefficient,a,false),false);
        result[row].high=add_bound(result[row].high,multiply_bound(coefficient,b,true),true);
    }
    return result;
}
bool outside_group_float(const WorldInstance& instance,const WorldFaceGroup& group,const WorldMatrix& view){
    std::array<FloatInterval,4> bounds{};bounds[3]={1,1};bool first=true;
    for(std::size_t i=group.first_face;i<std::size_t(group.first_face)+group.face_count;++i)
        for(auto index:instance.model.faces[i].indices){const auto p=instance.model.vertices[index];
            const std::array<float,3> v{p.x,p.y,p.z};
            for(unsigned axis=0;axis<3;++axis){
                if(first)bounds[axis]={v[axis],v[axis]};
                else{bounds[axis].low=std::min(bounds[axis].low,v[axis]);bounds[axis].high=std::max(bounds[axis].high,v[axis]);}}
            first=false;
        }
    const auto clip=transform_bounds_float(view,transform_bounds_float(instance.object_to_world,bounds));
    for(const auto& range:clip){
        const auto low=std::bit_cast<std::uint32_t>(range.low),high=std::bit_cast<std::uint32_t>(range.high);
        const auto lm=low&0x7fffffffu,hm=high&0x7fffffffu;
        // Uncertain/subnormal/over-limit intervals must reach the ordinary
        // vertex validator, so culling never hides a transform-domain error.
        if(lm>0x47800000u||hm>0x47800000u||!((lm==0&&hm==0)||
            (!(low>>31)&&lm>=0x00800000u&&!(high>>31)&&hm>=lm)||
            ((high>>31)&&hm>=0x00800000u&&(low>>31)&&lm>=hm)))return false;
    }
    const auto x=clip[0],y=clip[1],z=clip[2],w=clip[3];
    // Comparisons avoid rounded plane sums. Each strict inequality proves an
    // entire conservative clip interval lies outside the corresponding plane.
    return w.high<clip_min_w||x.high < -w.high||w.high<x.low||
        y.high < -w.high||w.high<y.low||z.high<0||w.high<z.low;
}
}
WorldScene::WorldScene()noexcept=default;WorldScene::~WorldScene()=default;
const char* world_scene_error_message(WorldSceneError e)noexcept{switch(e){
    case WorldSceneError::none:return "NONE";case WorldSceneError::not_configured:return "WORLD FRAME STORAGE NOT CONFIGURED";
    case WorldSceneError::invalid_capacity:return "INVALID WORLD FRAME CAPACITY";case WorldSceneError::invalid_model:return "INVALID WORLD MODEL";
    case WorldSceneError::invalid_transform:return "INVALID WORLD TRANSFORM";case WorldSceneError::input_limit:return "WORLD INPUT CAPACITY EXCEEDED";
    case WorldSceneError::output_limit:return "WORLD OUTPUT CAPACITY EXCEEDED";case WorldSceneError::clipping_failed:return "WORLD CLIPPING FAILED";
    default:return "WORLD FRAME ALLOCATION FAILED";}}
bool WorldScene::configure(std::size_t faces,std::size_t triangles,WorldSceneError& error,bool cache_vertices)noexcept{
    if(!faces||faces>32768||!triangles||triangles>16384)return fail(WorldSceneError::invalid_capacity,error);
    try{auto replacement=std::make_unique<Storage>();replacement->source_capacity=faces;replacement->output_capacity=triangles;
        replacement->stats.persistent_bytes=sizeof(Storage);
        for(auto& bank:replacement->banks){bank.materials.resize(faces);bank.triangles.resize(triangles);
            replacement->stats.persistent_bytes+=bank.materials.capacity()*sizeof(Material)+bank.triangles.capacity()*sizeof(Triangle);}
        if(cache_vertices){std::size_t entries=1;while(entries<faces*3&&entries<4096)entries*=2;
            replacement->vertex_cache.resize(entries);
            replacement->stats.persistent_bytes+=replacement->vertex_cache.capacity()*sizeof(Storage::CachedVertex);}
        storage_.swap(replacement);error=WorldSceneError::none;return true;
    }catch(const std::bad_alloc&){return fail(WorldSceneError::allocation_failed,error);}
    catch(const std::length_error&){return fail(WorldSceneError::allocation_failed,error);}
}
void WorldScene::set_profile_clock(WorldSceneClock clock,void* context)noexcept{clock_=clock;clock_context_=context;}
void WorldScene::set_transform_provider(WorldSceneTransformBatch provider,void* context)noexcept{transform_batch_=provider;transform_context_=context;}
void WorldScene::set_numeric_mode(WorldNumericMode mode)noexcept{numeric_mode_=mode;}
bool WorldScene::update(std::span<const WorldInstance> instances,const WorldView& view,WorldSceneError& error)noexcept{
    const bool native=numeric_mode_==WorldNumericMode::native_float;
    const auto now=[&]()noexcept{return clock_?clock_(clock_context_):std::uint64_t{0};};
    const auto validation_start=now();
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
    const auto validation_ticks=now()-validation_start;
    auto& bank=storage_->banks[storage_->active^1];std::size_t material_index=0,triangle_count=0;
    std::uint32_t groups=0,culled_groups=0,transformed_vertices=0,cache_hits=0;
    std::uint32_t batch_calls=0,batch_vertices=0,accepted_batch_vertices=0,failed_batches=0,clipped_batch_fallbacks=0;
    std::uint32_t provider_triangle_corners=0;
    std::uint64_t bounds_ticks=0,transform_ticks=0,clipping_ticks=0;
    auto& batch=storage_->batch;
    // Every face overwrites all input corners. Clippers commit only their
    // returned output count, so reuse scratch without clearing its unused tail.
    std::array<Triangle,clip_triangle_capacity> clipped;
    ClipTriangle input;
    for(const auto& instance:instances){std::size_t group_index=0;
    batch.count=0;batch.end_face=0;
    if(++storage_->cache_serial==approximate_vertex){for(auto& entry:storage_->vertex_cache)entry.serial=0;storage_->cache_serial=1;}
    for(std::size_t face_index=0;face_index<instance.model.faces.size();++face_index){
        if(group_index<instance.model.groups.size()&&instance.model.groups[group_index].first_face==face_index){
            const auto group=instance.model.groups[group_index++];++groups;
            const auto start=now();const bool outside=native?outside_group_float(instance,group,view.world_to_clip):outside_group(instance,group,view.world_to_clip);bounds_ticks+=now()-start;
            if(outside){
                ++culled_groups;material_index+=group.face_count;face_index+=group.face_count-1;continue;
            }
        }
        const auto& face=instance.model.faces[face_index];
        auto& material=bank.materials[material_index++];material.color=face.argb;
        input.material=&material;
        std::array<bool,3> approximate{};
        const auto transform_start=now();
        if(transform_batch_&&face_index>=batch.end_face){
            batch.count=0;batch.end_face=std::min(instance.model.faces.size(),face_index+ps2::vu1_transform_capacity/3);
            // Never prefetch through an unclassified group. The next block
            // first runs the same conservative culling decision as scalar mode.
            if(group_index<instance.model.groups.size())batch.end_face=std::min(batch.end_face,std::size_t(instance.model.groups[group_index].first_face));
            for(auto f=face_index;f<batch.end_face;++f)for(auto index:instance.model.faces[f].indices){
                if(!storage_->vertex_cache.empty()){const auto& entry=storage_->vertex_cache[index&(storage_->vertex_cache.size()-1)];
                    if((entry.serial&cache_serial_mask)==storage_->cache_serial&&entry.index==index)continue;}
                bool found=false;for(std::size_t k=0;k<batch.count;++k)if(batch.indices[k]==index){found=true;break;}
                if(found)continue;
                const auto p=instance.model.vertices[index];
                batch.indices[batch.count]=index;batch.input[batch.count]={p.x,p.y,p.z,1};batch.accepted[batch.count]=0;++batch.count;
            }
            if(batch.count){
                ++batch_calls;batch_vertices+=std::uint32_t(batch.count);
                const bool completed=transform_batch_(transform_context_,instance.object_to_world,view.world_to_clip,
                    std::span(batch.input).first(batch.count),std::span(batch.output).first(batch.count),std::span(batch.accepted).first(batch.count));
                if(!completed)++failed_batches;
                for(std::size_t k=0;k<batch.count;++k){
                    if(completed&&batch.accepted[k]==1&&valid_batch_result(batch.output[k],native))++accepted_batch_vertices;
                    else batch.accepted[k]=0;
                }
            }
        }
        for(unsigned i=0;i<3;++i){const auto index=face.indices[i];Storage::CachedVertex* cached=nullptr;
            if(!storage_->vertex_cache.empty()){
                cached=&storage_->vertex_cache[index&(storage_->vertex_cache.size()-1)];
                if((cached->serial&cache_serial_mask)==storage_->cache_serial&&cached->index==index){
                    input.vertices[i]=cached->clip;approximate[i]=(cached->serial&approximate_vertex)!=0;++cache_hits;continue;}
            }
            bool accepted=false;
            if(transform_batch_)for(std::size_t k=0;k<batch.count;++k){
                if(batch.indices[k]==index){if(batch.accepted[k]){const auto v=batch.output[k];input.vertices[i]={v.x,v.y,v.z,v.w,0,0};accepted=true;approximate[i]=true;}break;}
            }
            if(!accepted){
            ++transformed_vertices;
            if(!scalar_vertex(instance,view.world_to_clip,index,native,input.vertices[i]))return fail(WorldSceneError::invalid_transform,error);
            }
            if(cached){cached->clip=input.vertices[i];cached->index=index;cached->serial=storage_->cache_serial|(approximate[i]?approximate_vertex:0);}
        }
        if(!native&&(approximate[0]||approximate[1]||approximate[2])){
            bool clipping=false;
            for(const auto& v:input.vertices)if(!valid_batch_result({v.x,v.y,v.z,v.w})){clipping=true;break;}
            if(clipping){
                // A nearly parallel clipped edge can magnify a small endpoint
                // error. Per-vertex projection budgets therefore only permit
                // approximate results on wholly inside triangles. Restore all
                // approximate corners here, including those reused from cache.
                ++clipped_batch_fallbacks;
                for(unsigned i=0;i<3;++i)if(approximate[i]){
                    const auto index=face.indices[i];++transformed_vertices;
                    if(!scalar_vertex(instance,view.world_to_clip,index,false,input.vertices[i]))return fail(WorldSceneError::invalid_transform,error);
                    if(!storage_->vertex_cache.empty()){auto& entry=storage_->vertex_cache[index&(storage_->vertex_cache.size()-1)];
                        entry.clip=input.vertices[i];entry.index=index;entry.serial=storage_->cache_serial;}
                    approximate[i]=false;
                }
            }
        }
        transform_ticks+=now()-transform_start;
        const auto clip_start=now();
        const auto result=native?clip_project_triangle_float(input,clipped):clip_project_triangle(input,clipped);
        clipping_ticks+=now()-clip_start;
        if(result.status!=ClipStatus::success)return fail(WorldSceneError::clipping_failed,error);
        if(result.count>storage_->output_capacity-triangle_count)return fail(WorldSceneError::output_limit,error);
        if(result.count)for(bool used:approximate)provider_triangle_corners+=unsigned(used);
        for(std::size_t i=0;i<result.count;++i)bank.triangles[triangle_count++]=clipped[i];
    }}
    storage_->active^=1;storage_->count=triangle_count;storage_->clear=view.clear_color;
    storage_->stats={std::uint32_t(instances.size()),std::uint32_t(vertices),std::uint32_t(faces),std::uint32_t(triangle_count),storage_->stats.persistent_bytes};
    storage_->stats.groups=groups;storage_->stats.culled_groups=culled_groups;storage_->stats.transformed_vertices=transformed_vertices;
    storage_->stats.vertex_cache_hits=cache_hits;storage_->stats.validation_ticks=validation_ticks;
    storage_->stats.batch_calls=batch_calls;storage_->stats.batch_vertices=batch_vertices;
    storage_->stats.accepted_batch_vertices=accepted_batch_vertices;storage_->stats.failed_batches=failed_batches;
    storage_->stats.clipped_batch_fallbacks=clipped_batch_fallbacks;
    storage_->stats.provider_triangle_corners=provider_triangle_corners;
    storage_->stats.bounds_ticks=bounds_ticks;storage_->stats.transform_ticks=transform_ticks;storage_->stats.clipping_ticks=clipping_ticks;
    storage_->stats.numeric_mode=numeric_mode_;
    error=WorldSceneError::none;return true;
}
Frame WorldScene::frame()const noexcept{return storage_?Frame{{storage_->banks[storage_->active].triangles.data(),storage_->count},storage_->clear,true}:Frame{};}
WorldSceneStats WorldScene::stats()const noexcept{return storage_?storage_->stats:WorldSceneStats{};}
} // namespace ps2::native
