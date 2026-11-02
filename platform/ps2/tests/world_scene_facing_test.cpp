#include "world_scene.h"
#include <array>
#include <bit>
#include <cstdio>

namespace {
using namespace ps2::native;
unsigned checks=0;
void require(bool value){++checks;if(!value)throw checks;}
WorldMatrix identity(){WorldMatrix m{};m[0]=m[5]=m[10]=m[15]=1;return m;}
float number(std::uint32_t bits){return std::bit_cast<float>(bits);}
}
int main(){try{
    for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float}){
        WorldScene scene;WorldSceneError error;require(scene.configure(2,16,error));scene.set_numeric_mode(mode);
        std::array<WorldPoint,4> points{{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}}};
        std::array<WorldFace,2> faces{};faces[0].indices={0,1,2};faces[1].indices={0,2,3};
        for(auto& f:faces)f.facing={{0,0,1},{0,0,.5f},true};
        WorldInstance instance{{points,faces},identity()};WorldView view;view.world_to_clip=identity();
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        view.use_facing=true;
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        for(auto& f:faces)f.facing.normal.z=-1;
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.empty());
        require(scene.stats().backface_rejected_faces==2);
        view.use_facing=false;
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        require(scene.stats().backface_rejected_faces==0);
        view.use_facing=true;
        // Authored vector/anchor, not winding, decides visibility. Both fans
        // retain a deliberately different source anchor from their corners.
        for(auto& f:faces)f.facing={{0,0,-1},{0,0,-.5f},true};
        std::swap(faces[1].indices[1],faces[1].indices[2]);
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        for(auto& f:faces)f.facing.normal.z=number(0x80000000u);
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        // Applying translation to the normal would incorrectly flip this.
        for(auto& p:points)p.z=2;
        for(auto& f:faces)f.facing={{0,0,1},{0,0,2},true};
        instance.object_to_world[14]=-1.5f;
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        view.facing_origin={0,0,1};
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.empty());
        // A 90-degree instance turn changes the pre-projection origin test.
        instance.object_to_world=identity();instance.object_to_world[0]=0;instance.object_to_world[2]=-1;
        instance.object_to_world[8]=1;instance.object_to_world[10]=0;instance.object_to_world[12]=-.5f;
        for(auto& p:points){const float oldx=p.x;p.x=-.5f;p.z=oldx+.5f;}
        for(auto& f:faces)f.facing={{0,0,1},{0,0,1},true};
        view.facing_origin={0,0,0};
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        view.facing_origin={1,0,0};
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.empty());
        // Restore an accepted frame, then prove invalid facing work cannot
        // publish a partial new bank, even after an earlier face was valid.
        points={WorldPoint{-.5f,-.5f,.5f},WorldPoint{.5f,-.5f,.5f},WorldPoint{.5f,.5f,.5f},WorldPoint{-.5f,.5f,.5f}};
        instance.object_to_world=identity();view.facing_origin={};
        for(auto& f:faces)f.facing={{0,0,1},{0,0,.5f},true};
        require(scene.update(std::span(&instance,1),view,error));const auto old=scene.frame();
        faces[1].facing.normal.x=number(0x7fc12345);
        require(!scene.update(std::span(&instance,1),view,error));require(error==WorldSceneError::invalid_model);
        require(scene.frame().triangles.data()==old.triangles.data()&&scene.frame().triangles.size()==old.triangles.size());
        faces[1].facing.normal={number(0x00800000),0,0};instance.object_to_world[0]=.5f;
        require(!scene.update(std::span(&instance,1),view,error));require(error==WorldSceneError::invalid_transform);
        require(scene.frame().triangles.data()==old.triangles.data());
        // Disabled authored data remains unread, and the origin is relevant
        // only when the view explicitly enables facing.
        for(auto& f:faces){f.facing.enabled=false;f.facing.normal.x=number(0x7fc12345);}
        view.use_facing=false;view.facing_origin.x=number(0x7fc12345);
        require(scene.update(std::span(&instance,1),view,error));require(scene.frame().triangles.size()==2);
        view.use_facing=true;
        require(!scene.update(std::span(&instance,1),view,error));require(error==WorldSceneError::invalid_transform);
        // Prepared storage owns exactly the same authored plane values.
        view.facing_origin={};instance.object_to_world=identity();
        for(auto& f:faces)f.facing={{0,0,-1},{0,0,.5f},true};
        PreparedWorldModel owned;require(owned.assign(instance.model,error));
        for(auto& f:faces)f.facing.normal.z=1;
        PreparedWorldInstance prepared{&owned,identity()};
        require(scene.update_prepared(std::span(&prepared,1),view,error));require(scene.frame().triangles.empty());
    }
    // A reused plane is local to one instance, even when both instances borrow
    // the same face array. All geometry remains inside in either transform.
    for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float}){
        WorldScene scene;WorldSceneError error;require(scene.configure(3,8,error));scene.set_numeric_mode(mode);
        std::array<WorldPoint,3> points{{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f}}};
        std::array<WorldFace,3> faces{};
        for(auto& face:faces){face.indices={0,1,2};face.facing={{0,0,1},{0,0,0},true};}
        WorldView view;view.world_to_clip=identity();view.use_facing=true;
        std::array<WorldInstance,2> instances{{{{points,std::span(faces.data(),1)},identity()},{{points,std::span(faces.data(),1)},identity()}}};
        instances[0].object_to_world[14]=.25f;instances[1].object_to_world[14]=-.25f;
        require(scene.update(instances,view,error));require(scene.frame().triangles.size()==1);
        require(scene.stats().backface_rejected_faces==1);
        std::swap(instances[0],instances[1]);
        require(scene.update(instances,view,error));require(scene.frame().triangles.size()==1);
        require(scene.stats().backface_rejected_faces==1);
        // Every component of the six-word key matters. Following a front face,
        // change one component to make a back face, then repeat that back face.
        WorldInstance one{{points,faces},identity()};
        for(unsigned component=0;component<6;++component){
            for(auto& face:faces)face.facing={{1,1,1},{1,1,1},true};
            std::array<float*,6> fields{&faces[1].facing.normal.x,&faces[1].facing.normal.y,&faces[1].facing.normal.z,
                &faces[1].facing.point.x,&faces[1].facing.point.y,&faces[1].facing.point.z};
            *fields[component]=-8;faces[2].facing=faces[1].facing;
            require(scene.update(std::span(&one,1),view,error));require(scene.frame().triangles.size()==1);
            require(scene.stats().backface_rejected_faces==2);
        }
    }
    std::printf("WORLD_SCENE_FACING PASS checks=%u\n",checks);
}catch(unsigned n){std::fprintf(stderr,"WORLD_SCENE_FACING FAIL check=%u\n",n);return 1;}return 0;}
