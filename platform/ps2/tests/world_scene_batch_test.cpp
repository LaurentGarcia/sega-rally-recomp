#include "world_scene.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

namespace {unsigned allocations=0;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"WORLD_BATCH FAIL checks=%u frame=%u\n",checks,frames);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void equal(Frame a,Frame b){
    require(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
    for(std::size_t i=0;i<a.triangles.size();++i){const auto& x=a.triangles[i];const auto& y=b.triangles[i];require(x.material->color==y.material->color);
        for(unsigned j=0;j<3;++j){const auto p=x.vertices[j],q=y.vertices[j];const float left[]{p.x,p.y,p.u,p.v,p.q,p.depth},right[]{q.x,q.y,q.u,q.v,q.q,q.depth};
            for(unsigned k=0;k<6;++k)require(std::bit_cast<std::uint32_t>(left[k])==std::bit_cast<std::uint32_t>(right[k]));}
    }
}
struct Context {unsigned mode=0,calls=0,largest=0;};
bool provider(void* opaque,const WorldMatrix& a,const WorldMatrix& b,std::span<const ps2::TransformVector4> input,
              std::span<ps2::TransformVector4> output,std::span<std::uint8_t> accepted)noexcept{
    auto& context=*static_cast<Context*>(opaque);++context.calls;context.largest=std::max(context.largest,unsigned(input.size()));
    require(!input.empty()&&input.size()<=96&&input.size()==output.size()&&input.size()==accepted.size());
    require((reinterpret_cast<std::uintptr_t>(input.data())&15)==0&&(reinterpret_cast<std::uintptr_t>(output.data())&15)==0);
    const auto group=[](float y){return y<-.25f?0:y<.25f?1:2;};
    for(std::size_t i=0;i<input.size();++i){
        require(input[i].w==1&&group(input[i].y)==group(input[0].y));
        for(std::size_t j=0;j<i;++j)require(input[i].x!=input[j].x||input[i].y!=input[j].y||input[i].z!=input[j].z);
        const std::array<double,4> p{input[i].x,input[i].y,input[i].z,1};std::array<double,4> world{},clip{};
        for(unsigned row=0;row<4;++row)for(unsigned k=0;k<4;++k)world[row]+=double(a[row+4*k])*p[k];
        for(unsigned row=0;row<4;++row)for(unsigned k=0;k<4;++k)clip[row]+=double(b[row+4*k])*world[k];
        output[i]={float(clip[0]),float(clip[1]),float(clip[2]),float(clip[3])};accepted[i]=1;
        if(context.mode==1)accepted[i]=std::uint8_t(i&1);
        if(context.mode==2)output[i]={0,0,0,0}; // Failure outputs must be ignored.
        if(context.mode==3){switch(i%4){
            case 0:output[i].x=std::bit_cast<float>(0x7fc12345u);break;
            case 1:output[i].x=std::bit_cast<float>(1u);break;
            case 2:output[i].w=0;break;
            default:accepted[i]=2;break;}}
    }
    return context.mode!=2;
}
struct Perturbation {bool w=false;float amount=0;};
bool perturbed_provider(void* opaque,const WorldMatrix& a,const WorldMatrix& b,std::span<const ps2::TransformVector4> input,
                        std::span<ps2::TransformVector4> output,std::span<std::uint8_t> accepted)noexcept{
    const auto perturb=*static_cast<Perturbation*>(opaque);
    for(std::size_t i=0;i<input.size();++i){
        const std::array<double,4> p{input[i].x,input[i].y,input[i].z,1};std::array<double,4> world{},clip{};
        for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)world[r]+=double(a[r+4*k])*p[k];
        for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)clip[r]+=double(b[r+4*k])*world[k];
        output[i]={float(clip[0]),float(clip[1]),float(clip[2]),float(clip[3])};
        if(perturb.w)output[i].w+=perturb.amount;else output[i].x+=perturb.amount;
        accepted[i]=1;
    }
    return true;
}
void run(){
    std::vector<WorldPoint> points;std::vector<WorldFace> faces;std::vector<WorldFaceGroup> groups;
    const unsigned sizes[]{40,65,20};
    for(unsigned group=0;group<3;++group){const auto first=std::uint32_t(faces.size());
        for(unsigned i=0;i<sizes[group];++i){const auto v=std::uint32_t(points.size());const float x=-.375f+float(i)/128,y=-.5f+float(group)*.5f+float(i)/1024,z=.25f+float(i)/1024;
            points.insert(points.end(),{{x,y,z},{x+1.0f/256,y,z},{x,y+1.0f/256,z}});
            auto indices=std::array<std::uint32_t,3>{v,v+1,v+2};if(i>32&&i%5==0)indices=faces.back().indices;
            faces.push_back({indices,0xff000000u+std::uint32_t(faces.size())});
        }
        groups.push_back({first,sizes[group]});
    }
    std::array<WorldInstance,2> instances{{{{points,faces,groups},identity()},{{points,faces,groups},identity()}}};
    WorldScene reference,batched;WorldSceneError error;WorldView view{identity()};Context context;
    require(reference.configure(faces.size()*2,2000,error,false)&&batched.configure(faces.size()*2,2000,error));
    batched.set_transform_provider(provider,&context);
    const auto compare=[&](unsigned count){++frames;WorldSceneError a,b;const auto before=allocations;
        const bool x=reference.update(std::span(instances).first(count),view,a),y=batched.update(std::span(instances).first(count),view,b);
        require(x==y&&a==b);require(allocations==before);equal(reference.frame(),batched.frame());return x;};
    require(compare(1));require(context.largest==96&&batched.stats().batch_calls==6&&batched.stats().transformed_vertices==0);
    require(batched.stats().batch_vertices==batched.stats().accepted_batch_vertices&&batched.stats().failed_batches==0);
    instances[1].object_to_world[12]=.125f;
    for(unsigned mode=0;mode<4;++mode){context.mode=mode;
        for(unsigned n=0;n<24;++n){instances[0].object_to_world[12]=float(int(n%5)-2)*.0625f;view.world_to_clip[13]=float(int(n%7)-3)*.03125f;
            require(compare(2));if(mode==2)require(batched.stats().failed_batches==batched.stats().batch_calls&&batched.stats().accepted_batch_vertices==0);
            if(mode==3)require(batched.stats().accepted_batch_vertices==0&&batched.stats().transformed_vertices>0&&batched.stats().failed_batches==0);
        }
    }
    // An accepted result can never be reused after provider/model changes.
    batched.set_transform_provider(nullptr);points[0].x+=.015625f;require(compare(2));require(batched.stats().batch_calls==0);
    context.mode=0;batched.set_transform_provider(provider,&context);require(compare(2));
    const auto calls=context.calls;points[0].x=std::bit_cast<float>(0x7fc12345u);require(!compare(2));require(context.calls==calls);points[0].x=-.375f;
    view.world_to_clip=identity();view.world_to_clip[12]=100;view.world_to_clip[13]=100;require(compare(2));require(batched.stats().batch_calls==0);
    require(reference.configure(faces.size()*2,1,error,false)&&batched.configure(faces.size()*2,1,error));view.world_to_clip=identity();require(!compare(2));
    // Small accepted endpoint errors can greatly move an intersection on an
    // almost parallel edge. The preceding inside face also puts approximate
    // shared vertices in the cache; the clipped face must restore exact values.
    const std::array<WorldFace,2> edge_faces{{{{0,2,3},0xff123456},{{0,1,2},0xff456789}}};
    for(unsigned plane=0;plane<2;++plane){
        std::array<WorldPoint,4> edge_points{{{1-0x1p-14f,-.5f,.5f},{1+0x1p-14f,.5f,.5f},{.5f,.5f,.5f},{.5f,-.5f,.5f}}};
        Perturbation perturb{false,-0x1p-15f};WorldView edge_view{identity()};
        if(plane){edge_points={{{-0x1p-12f,-0x1p-12f,clip_min_w+0x1p-22f},{0x1p-12f,0x1p-12f,clip_min_w-0x1p-22f},
            {-0x1p-12f,0x1p-12f,0x1p-9f},{-0x1p-12f,-0x1p-12f,0x1p-9f}}};
            edge_view.world_to_clip[10]=.5f;edge_view.world_to_clip[11]=1;edge_view.world_to_clip[15]=0;perturb={true,0x1p-23f};}
        WorldInstance edge{{edge_points,edge_faces},identity()};
        require(reference.configure(2,16,error,false)&&batched.configure(2,16,error));batched.set_transform_provider(perturbed_provider,&perturb);
        ++frames;const auto allocation_count=allocations;
        require(reference.update({&edge,1},edge_view,error)&&batched.update({&edge,1},edge_view,error));require(allocations==allocation_count);
        const auto exact=reference.frame(),approximate=batched.frame();require(exact.triangles.size()==3&&approximate.triangles.size()==3);
        require(exact.triangles[0].vertices[0].x!=approximate.triangles[0].vertices[0].x);
        require(batched.stats().clipped_batch_fallbacks==1&&batched.stats().transformed_vertices==3&&batched.stats().provider_triangle_corners==3);
        equal({exact.triangles.subspan(1),exact.clear_color,exact.depth_test},{approximate.triangles.subspan(1),approximate.clear_color,approximate.depth_test});
        batched.set_transform_provider(nullptr);++frames;require(batched.update({&edge,1},edge_view,error));equal(exact,batched.frame());
    }
    std::printf("WORLD_BATCH PASS frames=%u checks=%u bounded_batches=96 guarded_fallback=1 no_update_allocations=1\n",frames,checks);
}
}
int main(){run();}
