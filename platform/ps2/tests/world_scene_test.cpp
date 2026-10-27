#include "world_scene.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
namespace {long failure=-1;unsigned allocations=0;}
void* operator new(std::size_t n){++allocations;if(failure==0)throw std::bad_alloc();if(failure>0)--failure;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;unsigned checks=0;
void require(bool x){++checks;if(!x){std::fprintf(stderr,"WORLD_SCENE FAIL check=%u\n",checks);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void run(){
    const std::array<WorldPoint,3> points{{{-.25f,-.25f,.5f},{.25f,-.25f,.5f},{0,.25f,.5f}}};
    const std::array<WorldFace,1> faces{{{{0,1,2},0xffb3c4d5}}};
    WorldInstance instance{{points,faces},identity()};WorldView view{identity()};WorldScene scene;WorldSceneError error;
    require(!scene.update({&instance,1},view,error)&&error==WorldSceneError::not_configured);
    require(scene.configure(2,16,error));require(scene.frame().triangles.empty());
    const auto before_allocations=allocations;require(scene.update({&instance,1},view,error));require(allocations==before_allocations);
    auto frame=scene.frame();require(frame.depth_test&&frame.triangles.size()==1);
    require(frame.triangles[0].vertices[0].x==186&&frame.triangles[0].vertices[0].y==240&&frame.triangles[0].vertices[0].depth==.5f);
    const auto initial=frame.triangles[0];instance.object_to_world[12]=.25f;
    require(scene.update({&instance,1},view,error));frame=scene.frame();
    for(unsigned i=0;i<3;++i){require(frame.triangles[0].vertices[i].x==initial.vertices[i].x+62);
        require(frame.triangles[0].vertices[i].y==initial.vertices[i].y&&frame.triangles[0].vertices[i].depth==initial.vertices[i].depth);}
    require(frame.triangles[0].material->color==faces[0].argb);
    // A second fixed instance never moves when the first instance is changed.
    std::array<WorldInstance,2> instances{instance,WorldInstance{{points,faces},identity()}};instances[1].object_to_world[12]=-.5f;
    require(scene.update(instances,view,error));const auto fixed=scene.frame().triangles[1];
    instances[0].object_to_world[12]=.125f;require(scene.update(instances,view,error));
    for(unsigned i=0;i<3;++i)require(scene.frame().triangles[1].vertices[i].x==fixed.vertices[i].x);
    // Independent asymmetric affine + view calculation, not a round trip using
    // the production helper: first input point projects to240.25,198,depth.625.
    instance.object_to_world={2,.25f,0,0,.5f,1,0,0,0,0,.5f,0,.125f,.25f,.125f,1};
    view.world_to_clip={.5f,0,0,0,0,.5f,0,0,.25f,0,1,0,.125f,0,0,1};
    require(scene.update({&instance,1},view,error));
    const auto p=scene.frame().triangles[0].vertices[0];require(p.x==240.25f&&p.y==198&&p.depth==.625f);
    const auto* retained=scene.frame().triangles.data();const auto* material=retained[0].material;
    const auto state=scene.stats();
    const auto preserved=[&](){require(scene.frame().triangles.data()==retained&&scene.frame().triangles[0].material==material&&scene.stats().output_triangles==state.output_triangles);};
    auto bad=instance;bad.object_to_world[3]=1;require(!scene.update({&bad,1},view,error)&&error==WorldSceneError::invalid_transform);preserved();
    auto bad_view=view;bad_view.world_to_clip[0]=std::bit_cast<float>(0x7fc00000u);require(!scene.update({&instance,1},bad_view,error));preserved();
    auto bad_points=points;bad_points[0].x=std::bit_cast<float>(1u);bad=instance;bad.model.vertices=bad_points;require(!scene.update({&bad,1},view,error)&&error==WorldSceneError::invalid_model);preserved();
    auto bad_faces=faces;bad_faces[0].indices[2]=3;bad=instance;bad.model.faces=bad_faces;require(!scene.update({&bad,1},view,error));preserved();
    const std::array<WorldInstance,3> too_many{instance,instance,instance};require(!scene.update(too_many,view,error)&&error==WorldSceneError::input_limit);preserved();
    for(long n=0;n<5;++n){failure=n;const bool ok=scene.configure(3,32,error);failure=-1;require(!ok&&error==WorldSceneError::allocation_failed);preserved();}
    require(!scene.configure(32769,16384,error)&&error==WorldSceneError::invalid_capacity);preserved();
    require(scene.configure(2,1,error));instance.object_to_world=identity();view.world_to_clip=identity();require(scene.update({&instance,1},view,error));
    const auto* limited=scene.frame().triangles.data();require(!scene.update(instances,view,error)&&error==WorldSceneError::output_limit);
    require(scene.frame().triangles.data()==limited&&scene.frame().triangles.size()==1);
    instance.object_to_world[12]=4;require(scene.update({&instance,1},view,error)&&scene.frame().triangles.empty());
    require(scene.update({},view,error)&&scene.stats().source_faces==0);
    std::printf("WORLD_SCENE PASS checks=%u fixed_translation_pixels=62 no_update_allocations=1\n",checks);
}
}
int main(){run();}
