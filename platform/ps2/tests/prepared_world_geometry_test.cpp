#include "world_scene.h"
#include "vu1_world.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace {
using namespace ps2;
using namespace ps2::native;
std::uint64_t checks=0,bytes_checked=0,hash=1469598103934665603ull;
void require(bool yes,unsigned line){++checks;if(!yes){std::fprintf(stderr,"PREPARED_GEOMETRY FAIL line=%u check=%llu\n",line,(unsigned long long)checks);std::exit(1);}}
#define R(x) require(bool(x),__LINE__)
static_assert(sizeof(Vu1WorldCorner)==32&&alignof(Vu1WorldCorner)==16);
static_assert(offsetof(Vu1WorldCorner,position)==0&&offsetof(Vu1WorldCorner,u)==16&&offsetof(Vu1WorldCorner,v)==20&&offsetof(Vu1WorldCorner,inv_width)==24&&offsetof(Vu1WorldCorner,inv_height)==28);
static_assert(std::is_same_v<decltype(std::declval<const PreparedVu1WorldCorners&>().corners()),std::span<const Vu1WorldCorner>>);
static_assert(!std::is_constructible_v<PreparedVu1WorldCorners,std::span<const Vu1WorldCorner>>);
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void same(std::span<const Vu1WorldCorner>a,std::span<const Vu1WorldCorner>b){
 R(a.size()==b.size());if(a.empty())return;R(std::memcmp(a.data(),b.data(),a.size_bytes())==0);
 const auto*p=reinterpret_cast<const unsigned char*>(a.data());bytes_checked+=a.size_bytes();
 for(std::size_t i=0;i<a.size_bytes();++i)hash=(hash^p[i])*1099511628211ull;
}
auto snapshot(const PreparedVu1WorldCorners& p){std::array<unsigned char,sizeof(p)> result;std::memcpy(result.data(),&p,sizeof(p));return result;}
std::vector<Vu1WorldCorner> old_gather(const WorldDrawInstance&i,std::size_t first,std::size_t count){
 std::vector<Vu1WorldCorner> out;out.reserve(count*3);
 for(std::size_t j=0;j<count;++j){const auto&f=i.faces[i.visible_face_indices[first+j]];
  const auto*t=f.material_index==world_flat_material?nullptr:i.materials[f.material_index].texture;
  for(unsigned k=0;k<3;++k){const auto p=i.vertices[f.indices[k]];
   out.push_back({{p.x,p.y,p.z,1},t?f.uv[k].u:0,t?f.uv[k].v:0,t?1.f/t->width:1,t?1.f/t->height:1});}
 }return out;
}
struct Source {
 std::array<std::uint8_t,32768> pixels{};
 std::array<Texture,36> textures{};std::array<Material,38> materials{};
 std::array<WorldPoint,8> points{{{-0.f,0.f,0x1p-126f},{65536.f,-65536.f,.5f},{-0x1p-126f,1,-0.f},{.25f,.5f,.75f},{-.25f,-.5f,-.75f},{1,2,3},{-1,-2,-3},{0,0,0}}};
 std::array<WorldFace,80> faces{};
 Source(){for(unsigned n=0;n<36;++n){const unsigned w=8u<<(n/6),h=8u<<(n%6);textures[n]={pixels.data(),std::size_t(w)*h/2,std::uint16_t(w),std::uint16_t(h),n+1};materials[n].texture=&textures[n];materials[n].palette.fill(0xff123456u);}
  materials[36].color=0xff556677u;materials[37].texture=&textures[0];materials[37].palette.fill(0xffabcdefu);
  const std::array<float,8>uv{-0.f,0.f,0x1p-126f,-0x1p-126f,65536.f,-65536.f,1.5f,-2.25f};
  for(unsigned n=0;n<faces.size();++n){auto&f=faces[n];f.indices={n%8,(n+1)%8,(n+3)%8};
   f.material_index=n%40<38?n%40:world_flat_material;
   for(unsigned k=0;k<3;++k)f.uv[k]={uv[(n+k)%uv.size()],uv[(n+3+k)%uv.size()]};}
 }
 WorldModel model(){return {points,faces,{},materials};}
};
void ranges_and_bytes(){Source source;PreparedWorldModel model;WorldScene scene;WorldSceneError error;
 R(model.assign(source.model(),error));R(scene.configure_plan(160,1280,error));
 std::array<PreparedWorldInstance,2> instances{{{&model,identity()},{&model,identity()}}};
 R(scene.plan_prepared(instances,{identity()},error));const auto plan=scene.draw_plan();
 for(const auto&i:plan.instances()){R(i.visible_face_indices.size()==80);R(i.texture_scales.size()==i.materials.size());
  for(std::size_t j=0;j<i.materials.size();++j){const auto*t=i.materials[j].texture;
   R(std::bit_cast<std::uint32_t>(i.texture_scales[j][0])==std::bit_cast<std::uint32_t>(t?1.f/t->width:1));
   R(std::bit_cast<std::uint32_t>(i.texture_scales[j][1])==std::bit_cast<std::uint32_t>(t?1.f/t->height:1));}}
 for(std::size_t instance=0;instance<2;++instance)for(std::size_t first=0;first<80;++first)
  for(std::size_t target=1;target<=16&&target<=80-first;++target){const auto expected=old_gather(plan.instances()[instance],first,target);
   for(std::size_t fill=0;fill<=target;++fill){PreparedVu1WorldCorners p;R(plan.begin_corners(instance,first,target,fill,p));
    R(p.ready()==(fill==target));same(p.corners(),std::span(expected).first(fill*3));
    R(valid_native_vu1_world_geometry(PreparedVu1WorldGeometry{identity(),identity(),p})==(fill==target));
    std::size_t have=fill;while(have<target){const auto take=std::min(target-have,1+(first+have)%4);R(plan.append_corners(instance,first+have,take,p));have+=take;
     R(p.ready()==(have==target));same(p.corners(),std::span(expected).first(have*3));}
    R(valid_native_vu1_world_geometry(Vu1WorldGeometry{identity(),identity(),p.corners()}));
   }
  }
 // Invalid ranges preserve every byte, both while partial and when ready.
 for(unsigned state=0;state<3;++state){PreparedVu1WorldCorners p;if(state)R(plan.begin_corners(0,0,16,state==1?7:16,p));const auto before=snapshot(p);
  const auto rejected=[&](bool accepted){R(!accepted);R(snapshot(p)==before);};const auto huge=std::numeric_limits<std::size_t>::max();
  for(auto bad:{std::size_t(2),huge}){rejected(plan.begin_corners(bad,0,1,1,p));rejected(plan.append_corners(bad,0,1,p));}
  for(auto bad:{std::size_t(81),huge}){rejected(plan.begin_corners(0,bad,1,1,p));rejected(plan.append_corners(0,bad,1,p));}
  for(auto target:{std::size_t(0),std::size_t(17),huge})rejected(plan.begin_corners(0,0,target,0,p));
  rejected(plan.begin_corners(0,0,1,2,p));rejected(plan.begin_corners(0,0,1,huge,p));rejected(plan.begin_corners(0,79,2,0,p));
  rejected(plan.append_corners(0,0,0,p));rejected(plan.append_corners(0,0,17,p));rejected(plan.append_corners(0,0,huge,p));
  if(state!=1)rejected(plan.append_corners(0,0,1,p));
 }
 // Prefixes may deliberately draw from different valid sources. They certify
 // the copied values, not an implicit plan/source identity or source order.
 PreparedVu1WorldCorners mixed;R(plan.begin_corners(0,2,4,1,mixed));R(plan.append_corners(1,50,3,mixed));
 auto expected=old_gather(plan.instances()[0],2,1),tail=old_gather(plan.instances()[1],50,3);expected.insert(expected.end(),tail.begin(),tail.end());same(mixed.corners(),expected);
}
void dynamic_admission(){Source source;PreparedWorldModel model;WorldScene scene;WorldSceneError error;R(model.assign(source.model(),error));R(scene.configure_plan(80,640,error));
 PreparedWorldInstance instance{&model,identity()};R(scene.plan_prepared({&instance,1},{identity()},error));PreparedVu1WorldCorners p;R(scene.draw_plan().begin_corners(0,0,16,16,p));
 std::array<Vu1WorldState,16>states{};for(auto&s:states){s.prim=19;s.rgba=0x80808080u;}
 const auto check=[&](WorldMatrix a,WorldMatrix b,unsigned height,std::span<const Vu1WorldState>s,Vu1WorldCompletion completion){
  const Vu1WorldGeometry raw{a,b,p.corners(),height};const PreparedVu1WorldGeometry prepared{a,b,p,height};
  R(valid_native_vu1_world_geometry(raw)==valid_native_vu1_world_geometry(prepared));
  const bool old=valid_native_vu1_world_input(Vu1WorldBatch{a,b,p.corners(),s,height,0,completion});
  R(old==valid_native_vu1_world_input(PreparedVu1WorldBatch{a,b,p,s,height,0,completion}));return old;};
 R(check(identity(),identity(),448,states,Vu1WorldCompletion::gs_finished));R(check(identity(),identity(),512,states,Vu1WorldCompletion::path1_retired));
 for(unsigned h:{0u,1u,447u,449u,511u,513u,~0u})R(!check(identity(),identity(),h,states,Vu1WorldCompletion::gs_finished));
 for(unsigned which=0;which<2;++which)for(unsigned slot=0;slot<16;++slot)
  for(auto bits:{0x00000001u,0x007fffffu,0x80000001u,0x47800001u,0xc7800001u,0x7f800000u,0xff800000u,0x7fc12345u}){
   auto a=identity(),b=identity();(which?b:a)[slot]=std::bit_cast<float>(bits);R(!check(a,b,448,states,Vu1WorldCompletion::gs_finished));}
 for(unsigned slot:{3u,7u,11u}){auto a=identity();a[slot]=-0.f;R(check(a,identity(),448,states,Vu1WorldCompletion::gs_finished));a[slot]=0x1p-126f;R(!check(a,identity(),448,states,Vu1WorldCompletion::gs_finished));}
 for(float last:{0.f,-0.f,-1.f,.5f,2.f}){auto a=identity();a[15]=last;R(!check(a,identity(),448,states,Vu1WorldCompletion::gs_finished));}
 R(!check(identity(),identity(),448,{},Vu1WorldCompletion::gs_finished));R(!check(identity(),identity(),448,std::span(states).first(15),Vu1WorldCompletion::gs_finished));
 for(unsigned i=0;i<16;++i){states[i].prim=4;R(!check(identity(),identity(),448,states,Vu1WorldCompletion::gs_finished));states[i].prim=3;
  states[i].rgba=0x81000000u;R(!check(identity(),identity(),448,states,Vu1WorldCompletion::gs_finished));states[i].rgba=0x80000000u;}
 R(!check(identity(),identity(),448,states,static_cast<Vu1WorldCompletion>(255)));
 PreparedVu1WorldCorners empty;R(!valid_native_vu1_world_geometry(PreparedVu1WorldGeometry{identity(),identity(),empty}));
 R(!valid_native_vu1_world_input(PreparedVu1WorldBatch{identity(),identity(),empty,{}}));
 R(valid_native_vu1_world_input(Vu1WorldBatch{identity(),identity(),{}, {}}));
}
void owned_copy(){PreparedVu1WorldCorners survivor;std::vector<Vu1WorldCorner>expected;
 {Source source;PreparedWorldModel model;WorldScene scene;WorldSceneError error;R(model.assign(source.model(),error));R(scene.configure_plan(80,640,error));
  PreparedWorldInstance instance{&model,identity()};R(scene.plan_prepared({&instance,1},{identity()},error));const auto plan=scene.draw_plan();
  expected=old_gather(plan.instances()[0],30,16);R(plan.begin_corners(0,30,16,16,survivor));source.points.fill({});source.pixels.fill(255);source.faces.fill({});}
 const auto copied=survivor;same(survivor.corners(),expected);same(copied.corners(),expected);
 R(valid_native_vu1_world_geometry(PreparedVu1WorldGeometry{identity(),identity(),copied}));
}
}
int main(){ranges_and_bytes();dynamic_admission();owned_copy();std::printf("PREPARED_GEOMETRY PASS checks=%llu bytes=%llu hash=%016llx owned_bytes=%zu byte_parity=1 partial_ranges=1 dynamic_admission=1\n",(unsigned long long)checks,(unsigned long long)bytes_checked,(unsigned long long)hash,sizeof(PreparedVu1WorldCorners));}
