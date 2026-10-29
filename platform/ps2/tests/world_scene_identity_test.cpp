#include "world_scene.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
using namespace ps2::native;
namespace {unsigned checks=0,frames=0;void require(bool yes){++checks;if(!yes){std::fprintf(stderr,"WORLD_IDENTITY FAIL %u\n",checks);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::uint64_t hash(Frame f){std::uint64_t h=1469598103934665603ull;auto add=[&](std::uint32_t x){h^=x;h*=1099511628211ull;};
 add(f.triangles.size());for(const auto&t:f.triangles){add(t.material->color);for(const auto&v:t.vertices)for(float x:{v.x,v.y,v.depth,v.u,v.v,v.q})add(std::bit_cast<std::uint32_t>(x));}return h;}
}
int main(){
 WorldScene old,fast;WorldSceneError a,b;require(old.configure(2,16,a));require(fast.configure(2,16,b));
 const std::array<WorldPoint,6>points{{{-.25f,-.25f,.5f},{.25f,-.25f,.5f},{-0.f,.25f,.5f},{2,-.25f,.75f},{2.25f,-.25f,.75f},{2,.25f,.75f}}};
 const std::array<WorldFace,2>faces{{{{0,1,2},0xff123456},{{3,4,5},0xffabcdef}}};const std::array<WorldFaceGroup,2>groups{{{0,1},{1,1}}};
 PreparedWorldModel model;require(model.assign({points,faces,groups},a));PreparedWorldInstance instance{&model,identity()};WorldInstance ordinary{{points,faces},identity()};WorldView view{identity()};
 const auto run=[&](){ordinary.object_to_world=instance.object_to_world;const bool x=old.update({&ordinary,1},view,a);const bool y=fast.update_prepared({&instance,1},view,b);
  require(x==y&&a==b);require(hash(old.frame())==hash(fast.frame()));++frames;};
 for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float}){old.set_numeric_mode(mode);fast.set_numeric_mode(mode);
  // Every combination of off-diagonal signed zeros is an exact identity.
  for(unsigned signs=0;signs<4096;++signs){instance.object_to_world=identity();unsigned bit=0;
   for(unsigned i=0;i<16;++i)if(i%5){instance.object_to_world[i]=std::bit_cast<float>((signs&(1u<<bit++))?0x80000000u:0u);}
   for(unsigned v=0;v<3;++v){view.world_to_clip=identity();view.world_to_clip[12]=float(int(v)-1);view.world_to_clip[1]=(v&1)?.25f:0;run();}
  }
  // Every one-bit perturbation, including tiny off-diagonal values and nearby
  // diagonal floats. Only a zero sign-bit change can remain an identity.
  for(unsigned index=0;index<16;++index)for(unsigned bit=0;bit<32;++bit){instance.object_to_world=identity();
   instance.object_to_world[index]=std::bit_cast<float>(std::bit_cast<std::uint32_t>(instance.object_to_world[index])^(1u<<bit));
   view.world_to_clip=identity();run();}
 }
 const float one_up=std::bit_cast<float>(0x3f800001u),two_up=std::bit_cast<float>(0x3f800002u);
 const std::array<WorldPoint,3>edge{{{one_up,-.25f,.5f},{two_up,-.25f,.5f},{one_up,.25f,.5f}}};
 const std::array<WorldFace,1>edge_face{{{{0,1,2},0xff123456}}};const std::array<WorldFaceGroup,1>edge_group{{{0,1}}};
 require(model.assign({edge,edge_face,edge_group},a));ordinary.model={edge,edge_face};view.world_to_clip=identity();instance.object_to_world=identity();
 instance.object_to_world[0]=std::bit_cast<float>(0x3f7ffffcu);run();require(!fast.frame().triangles.empty());
 std::printf("WORLD_IDENTITY PASS frames=%u checks=%u signed_zero_masks=4096 single_bit_mutations=512\n",frames,checks);
}
