#include "world_scene.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {unsigned allocations=0;long fail_allocation=-1;}
void* operator new(std::size_t n){++allocations;if(fail_allocation==0)throw std::bad_alloc();if(fail_allocation>0)--fail_allocation;
    if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"WORLD_CACHE FAIL checks=%u frame=%u\n",checks,frames);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void equal(Frame a,Frame b){
    require(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
    for(std::size_t i=0;i<a.triangles.size();++i){const auto& x=a.triangles[i];const auto& y=b.triangles[i];
        require(x.material->color==y.material->color);
        for(unsigned j=0;j<3;++j){const auto p=x.vertices[j],q=y.vertices[j];
            const float left[]{p.x,p.y,p.u,p.v,p.q,p.depth},right[]{q.x,q.y,q.u,q.v,q.q,q.depth};
            for(unsigned k=0;k<6;++k)require(std::bit_cast<std::uint32_t>(left[k])==std::bit_cast<std::uint32_t>(right[k]));
        }
    }
}
std::uint64_t clock_tick(void* p)noexcept{auto& count=*static_cast<std::uint64_t*>(p);count+=7;return count;}
void run(){
    std::array<WorldPoint,20> vertices{};
    vertices[0]={-.25f,-.25f,.5f};vertices[1]={.25f,-.25f,.5f};vertices[2]={.25f,.25f,.5f};vertices[3]={-.25f,.25f,.5f};
    vertices[16]={-.5f,-.5f,.75f};vertices[17]={0,-.5f,.75f};vertices[18]={0,0,.75f};
    const std::array<WorldFace,5> faces{{{{0,1,2},0xff123456},{{0,2,3},0xff234567},{{16,17,18},0xff345678},{{0,2,3},0xff456789},{{0,1,2},0xff56789a}}};
    std::array<WorldInstance,2> instances{{{{vertices,faces},identity()},{{vertices,faces},identity()}}};
    WorldScene uncached,cached;WorldSceneError error;WorldView view{identity()};
    require(uncached.configure(10,80,error,false)&&cached.configure(5,80,error));
    require(cached.stats().persistent_bytes-uncached.stats().persistent_bytes+10*sizeof(Material)==16*32);
    std::uint64_t ticks=0;cached.set_profile_clock(clock_tick,&ticks);
    const auto compare=[&](unsigned count){
        ++frames;const auto before=allocations;WorldSceneError a,b;
        const bool x=uncached.update(std::span(instances).first(count),view,a),y=cached.update(std::span(instances).first(count),view,b);
        require(x==y&&a==b);require(allocations==before);equal(uncached.frame(),cached.frame());
        return x;
    };
    require(compare(1));require(cached.stats().transformed_vertices==10&&cached.stats().vertex_cache_hits==5);
    require(uncached.stats().transformed_vertices==15&&uncached.stats().vertex_cache_hits==0);
    require(cached.stats().validation_ticks==7&&cached.stats().transform_ticks==35&&cached.stats().clipping_ticks==35&&cached.stats().bounds_ticks==0);
    // Configure allocates one cache after the frame banks. Failure must retain
    // the old cache, clock, material pointers and complete output frame.
    const auto* old=cached.frame().triangles.data();fail_allocation=5;const bool configured=cached.configure(10,80,error);fail_allocation=-1;
    require(!configured&&error==WorldSceneError::allocation_failed);require(cached.frame().triangles.data()==old);
    require(compare(1));
    require(cached.configure(10,80,error)); // New cache has32 entries; no collision.
    instances[1].object_to_world[12]=.5f;require(compare(2));
    require(cached.stats().transformed_vertices==14&&cached.stats().vertex_cache_hits==16);
    require(cached.stats().validation_ticks==7); // Clock survived configure.
    for(unsigned n=0;n<512;++n){
        vertices[0].x=-.25f+float(n%9)*.03125f;
        vertices[16].y=-.5f+float(n%7)*.03125f;
        instances[0].object_to_world[12]=float(int(n%17)-8)*.125f;
        instances[1].object_to_world[12]=-instances[0].object_to_world[12];
        view.world_to_clip[13]=float(int(n%13)-6)*.125f;
        require(compare(2));
    }
    // Previously cached valid values cannot mask a later invalid caller edit.
    vertices[1].x=std::bit_cast<float>(1u);require(!compare(2));vertices[1].x=.25f;
    view.world_to_clip=identity();instances[0].object_to_world[0]=65536;instances[0].object_to_world[12]=65536;require(!compare(2));
    instances[0].object_to_world=identity();require(compare(2));
    cached.set_profile_clock(nullptr);require(compare(2));require(cached.stats().validation_ticks==0&&cached.stats().transform_ticks==0&&cached.stats().clipping_ticks==0);
    std::printf("WORLD_CACHE PASS frames=%u checks=%u mutation_and_collision=1 no_update_allocations=1\n",frames,checks);
}
}
int main(){run();}
