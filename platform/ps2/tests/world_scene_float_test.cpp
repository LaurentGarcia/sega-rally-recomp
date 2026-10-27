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
unsigned checks=0,frames=0;std::uint64_t culled=0,provided=0;
void require(bool okay){++checks;if(!okay){std::fprintf(stderr,"WORLD_FLOAT FAIL check=%u frame=%u\n",checks,frames);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
bool same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void compare(Frame a,Frame b){
    require(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
    for(std::size_t i=0;i<a.triangles.size();++i){const auto& x=a.triangles[i];const auto& y=b.triangles[i];
        require(x.material->color==y.material->color&&x.material->texture==y.material->texture);
        for(unsigned j=0;j<3;++j){const auto p=x.vertices[j],q=y.vertices[j];
            require(same(p.x,q.x)&&same(p.y,q.y)&&same(p.u,q.u)&&same(p.v,q.v)&&same(p.q,q.q)&&same(p.depth,q.depth));}}
}
struct Provider {unsigned calls=0;bool fail=false,invalid=false;};
bool raw(void* context,const WorldMatrix& a,const WorldMatrix& b,std::span<const ps2::TransformVector4> input,
         std::span<ps2::TransformVector4> output,std::span<std::uint8_t> accepted)noexcept{
    auto& p=*static_cast<Provider*>(context);++p.calls;
    for(std::size_t i=0;i<input.size();++i){
        const auto in=std::bit_cast<std::array<float,4>>(input[i]);std::array<float,4> world{},clip{};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)world[row]+=a[row+4*col]*in[col];
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)clip[row]+=b[row+4*col]*world[col];
        if(p.invalid)clip[0]=std::bit_cast<float>(std::uint32_t(0x7fc00000));
        output[i]=std::bit_cast<ps2::TransformVector4>(clip);accepted[i]=1;
    }
    return !p.fail;
}
void run(){
    std::vector<WorldPoint> points;std::vector<WorldFace> faces;std::vector<WorldFaceGroup> groups;
    for(unsigned n=0;n<80;++n){const float x=float(int(n%10)-5)*3.1f,y=float(int(n/10)-4)*2.9f;
        const auto v=std::uint32_t(points.size()),f=std::uint32_t(faces.size());const float z=float(n%7)*.071f;
        points.insert(points.end(),{{x-.37f,y-.23f,z},{x+.31f,y-.19f,z+.11f},{x+.29f,y+.41f,z+.23f},{x-.33f,y+.35f,z+.17f}});
        faces.push_back({{v,v+1,v+2},0xff000000u+n});faces.push_back({{v,v+2,v+3},0xff002000u+n});groups.push_back({f,2});}
    WorldScene plain,grouped,provider;WorldSceneError error;Provider callback;
    for(auto* scene:{&plain,&grouped,&provider}){
        scene->set_numeric_mode(WorldNumericMode::native_float);
        require(scene->configure(faces.size(),1280,error,scene!=&plain));}
    provider.set_transform_provider(raw,&callback);
    WorldInstance ordinary{{points,faces},identity()},optimized{{points,faces,groups},identity()};WorldView view{identity()};
    auto step=[&](){++frames;WorldSceneError a,b,c;const auto count=allocations;
        const bool first=plain.update({&ordinary,1},view,a),second=grouped.update({&optimized,1},view,b),third=provider.update({&optimized,1},view,c);
        require(first==second&&a==b&&first==third&&a==c);require(allocations==count);
        compare(plain.frame(),grouped.frame());compare(plain.frame(),provider.frame());
        if(first){require(grouped.stats().numeric_mode==WorldNumericMode::native_float);
            require(provider.stats().clipped_batch_fallbacks==0);culled+=grouped.stats().culled_groups;provided+=provider.stats().provider_triangle_corners;}
    };
    step();std::uint32_t random=0x164ecd37;
    auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned i=0;i<768;++i){
        ordinary.object_to_world=identity();const float sine=float(int(next()%101)-50)*.017f,cosine=float(int(next()%101)-50)*.019f;
        ordinary.object_to_world[0]=cosine;ordinary.object_to_world[1]=sine;ordinary.object_to_world[4]=-sine;ordinary.object_to_world[5]=cosine;
        ordinary.object_to_world[12]=float(int(next()%65)-32)*.273f;ordinary.object_to_world[13]=float(int(next()%65)-32)*.191f;
        ordinary.object_to_world[14]=float(int(next()%17)-8)*.11f;optimized.object_to_world=ordinary.object_to_world;
        view.world_to_clip=identity();view.world_to_clip[0]=float(next()%8+1)*.127f;view.world_to_clip[5]=float(next()%8+1)*.139f;
        if(i%3==0){view.world_to_clip[11]=.923f;view.world_to_clip[15]=float(i%5)*.27f;}
        if(i%31==0)view.world_to_clip[14]=-0.f;
        step();}
    // Provider transport and invalid results use native float fallback, not the
    // reference transform; the same grouped frame must remain bit-identical.
    ordinary.object_to_world=optimized.object_to_world=identity();view.world_to_clip=identity();
    callback.fail=true;step();require(provider.stats().failed_batches>0);callback.fail=false;
    callback.invalid=true;step();require(provider.stats().accepted_batch_vertices==0);callback.invalid=false;
    // An outside group must not suppress a bad result on a different axis.
    view.world_to_clip[12]=128;view.world_to_clip[5]=0x1p-126f;step();
    view.world_to_clip=identity();view.world_to_clip[0]=65536;step();
    view.world_to_clip=identity();view.world_to_clip[15]=clip_min_w;step();
    // Explicit mode changes invalidate logical caches without reallocating or
    // reinterpreting an already-published frame. The mode survives configure.
    provider.set_transform_provider(nullptr);provider.set_numeric_mode(WorldNumericMode::reference);
    require(provider.configure(faces.size(),1280,error));view.world_to_clip=identity();
    require(provider.update({&ordinary,1},view,error)&&provider.stats().numeric_mode==WorldNumericMode::reference);
    provider.set_numeric_mode(WorldNumericMode::native_float);provider.set_transform_provider(raw,&callback);step();
    const auto retained=provider.frame().triangles.data();const auto count=provider.frame().triangles.size();
    const auto old_stats=provider.stats();points[0].x=std::bit_cast<float>(std::uint32_t(1));
    require(!provider.update({&optimized,1},view,error)&&error==WorldSceneError::invalid_model);
    require(provider.frame().triangles.data()==retained&&provider.frame().triangles.size()==count);
    require(provider.stats().output_triangles==old_stats.output_triangles&&provider.stats().numeric_mode==old_stats.numeric_mode);
    require(culled>10000&&provided>1000&&callback.calls>0);
    std::printf("WORLD_FLOAT PASS frames=%u checks=%u culled_groups=%llu provider_corners=%llu native_clipping=1 no_update_allocations=1\n",
        frames,checks,(unsigned long long)culled,(unsigned long long)provided);
}
}
int main(){run();}
