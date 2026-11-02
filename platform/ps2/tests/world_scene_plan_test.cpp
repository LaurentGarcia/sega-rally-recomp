#include "world_scene.h"
#include "native_geometry_float.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <vector>
namespace {std::size_t allocations=0;long fail_after=-1;}
void* operator new(std::size_t n){++allocations;if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0;
void require(bool yes,unsigned line){++checks;if(!yes){std::fprintf(stderr,"WORLD_PLAN FAIL line=%u check=%u\n",line,checks);std::exit(1);}}
#define R(x) require(bool(x),__LINE__)
static_assert(!std::is_aggregate_v<WorldDrawPlan>);
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::array<std::uint32_t,6> words(Vertex v){return {std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.depth),std::bit_cast<std::uint32_t>(v.u),std::bit_cast<std::uint32_t>(v.v),std::bit_cast<std::uint32_t>(v.q)};}
void equal(Frame a,Frame b){R(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
 for(std::size_t i=0;i<a.triangles.size();++i){const auto&x=a.triangles[i];const auto&y=b.triangles[i];const auto&m=*x.material;const auto&n=*y.material;
  R(m.color==n.color&&m.cutout==n.cutout&&m.checker==n.checker&&m.palette==n.palette&&bool(m.texture)==bool(n.texture));
  if(m.texture){R(m.texture->width==n.texture->width&&m.texture->height==n.texture->height&&m.texture->generation==n.texture->generation);const auto bytes=std::size_t(m.texture->width)*m.texture->height/2;R(std::equal(m.texture->indices,m.texture->indices+bytes,n.texture->indices));}
  for(unsigned k=0;k<3;++k)R(words(x.vertices[k])==words(y.vertices[k]));
 }
}
struct Source {
 std::array<std::uint8_t,32>pixels{};Texture texture{pixels.data(),pixels.size(),8,8,5};std::array<Material,2>materials{};
 std::array<WorldPoint,9>points{{{-.75f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f},{1.5f,-.25f,.5f},{.25f,.75f,.5f},{1.5f,.75f,.5f},{3,-.25f,.5f},{4,-.25f,.5f},{3,.25f,.5f}}};
 std::array<WorldFace,6>faces{};std::array<WorldFaceGroup,3>groups{{{0,3},{3,2},{5,1}}};
 Source(){for(unsigned i=0;i<pixels.size();++i)pixels[i]=std::uint8_t(i*9);for(unsigned i=0;i<2;++i){materials[i].texture=&texture;materials[i].color=0xff112233u+i;materials[i].palette.fill(0xff102030u+i);}materials[1].cutout=true;materials[1].checker=true;materials[1].palette[0]=0;
  for(unsigned i=0;i<6;++i){faces[i].indices={0,1,2};faces[i].argb=0xffaabb00u+i;faces[i].uv={{{float(i*8),0},{float(i*8+8),0},{float(i*8+4),8}}};faces[i].facing={{1,0,0},{float(i)/8,0,0},true};}
  faces[1].material_index=0;faces[2].material_index=1;faces[3].indices={1,3,4};faces[4].indices={3,5,4};faces[3].material_index=faces[4].material_index=1;faces[4].facing=faces[3].facing;faces[5].indices={6,7,8};
 }
 WorldModel model(){return {points,faces,groups,materials};}
};
std::array<float,4> transform(const WorldMatrix&m,const std::array<float,4>&p){std::array<float,4>q{};for(unsigned r=0;r<4;++r){const float x=m[r]*p[0],y=m[r+4]*p[1],z=m[r+8]*p[2],w=m[r+12]*p[3];const float zero_x=0.f+x,xy=zero_x+y,xyz=xy+z;q[r]=xyz+w;}return q;}
struct Expanded {
 std::vector<Material>materials;std::vector<Triangle>triangles;
 bool render(const WorldDrawPlan& plan){std::size_t faces=0;for(const auto&i:plan.instances())faces+=i.visible_face_indices.size();materials.resize(faces);triangles.clear();triangles.reserve(plan.output_triangle_capacity());std::size_t material=0;
  for(const auto&i:plan.instances()){std::uint32_t previous=0;bool first=true;for(auto index:i.visible_face_indices){R(index<i.faces.size()&&(first||index>previous));previous=index;first=false;const auto&f=i.faces[index];auto&m=materials[material++];m={};if(f.material_index==world_flat_material)m.color=f.argb;else m=i.materials[f.material_index];ClipTriangle input;input.material=&m;
   for(unsigned k=0;k<3;++k){const auto p=i.vertices[f.indices[k]];const auto q=transform(plan.world_to_clip(),transform(i.object_to_world,{p.x,p.y,p.z,1}));input.vertices[k]={q[0],q[1],q[2],q[3],m.texture?f.uv[k].u:0,m.texture?f.uv[k].v:0};}
   std::array<Triangle,8>output;const auto r=clip_project_triangle_float(input,output);if(r.status!=ClipStatus::success||r.count>plan.output_triangle_capacity()-triangles.size())return false;triangles.insert(triangles.end(),output.begin(),output.begin()+r.count);
  }}return true;
 }
 Frame frame(const WorldDrawPlan&p){return {triangles,p.clear_color(),p.depth_test()};}
};
void parity(){Source source;PreparedWorldModel model;WorldSceneError error;R(model.assign(source.model(),error));WorldScene normal,planner;normal.set_numeric_mode(WorldNumericMode::native_float);R(normal.configure(12,96,error));R(planner.configure_plan(12,96,error));
 std::array<PreparedWorldInstance,2>instances{{{&model,identity()},{&model,identity()}}};Expanded expanded;
 for(unsigned n=0;n<97;++n){WorldView view{identity(),0xff123456u};view.use_facing=true;view.facing_origin.x=float(n%9)/16;view.world_to_clip[11]=(n&1)?.125f:0;
  instances[0].object_to_world[12]=float(int(n%7)-3)/8;instances[1].object_to_world[12]=float(int(n%5)-2)/8;instances[1].object_to_world[0]=(n&2)?-1.f:1.f;
  R(normal.update_prepared(instances,view,error));const auto count=allocations;R(planner.plan_prepared(instances,view,error));R(allocations==count);
  const auto plan=planner.draw_plan();R(plan.instances().size()==2&&plan.output_triangle_capacity()==96&&plan.world_to_clip()==view.world_to_clip);
  R(expanded.render(plan));equal(normal.frame(),expanded.frame(plan));const auto a=normal.stats(),b=planner.plan_stats();R(a.source_faces==b.source_faces&&a.groups==b.groups&&a.culled_groups==b.culled_groups&&a.backface_rejected_faces==b.backface_rejected_faces);R(b.output_triangles==0&&b.transformed_vertices==0&&b.batch_calls==0);++frames;
 }
 // Empty selection still publishes a clear/depth plan with retained instances.
 WorldView outside{identity()};outside.use_facing=true;outside.facing_origin.x=100;R(planner.plan_prepared(instances,outside,error));for(const auto&i:planner.draw_plan().instances())R(i.visible_face_indices.empty());
 R(planner.plan_prepared({},outside,error));R(planner.draw_plan().instances().empty()&&planner.draw_plan().output_triangle_capacity()==96);
}
void lifetime_and_atomicity(){Source source;WorldScene scene;WorldSceneError error;WorldView view{identity()};view.use_facing=true;R(scene.draw_plan().output_triangle_capacity()==0);
 R(!scene.plan_prepared({},view,error)&&error==WorldSceneError::not_configured);R(scene.configure_plan(12,96,error));auto owner=std::make_unique<PreparedWorldModel>();R(owner->assign(source.model(),error));PreparedWorldInstance instance{owner.get(),identity()};R(scene.plan_prepared({&instance,1},view,error));const auto plan=scene.draw_plan();Expanded expected;R(expected.render(plan));const auto data=plan.instances().data();const auto stats=scene.plan_stats();
 // Same owner receives new storage, then dies: both flat positions and texture
 // data referenced by the prior plan stay alive through its snapshot lease.
 source.points[0].x+=.125f;source.pixels.fill(0);R(owner->assign(source.model(),error));owner.reset();Expanded after;R(after.render(plan));equal(expected.frame(plan),after.frame(plan));
 auto valid=std::make_unique<PreparedWorldModel>();R(valid->assign(source.model(),error));instance.model=valid.get();
 const auto retained=[&](){R(scene.draw_plan().instances().data()==data&&scene.plan_stats().source_faces==stats.source_faces);R(after.render(scene.draw_plan()));equal(expected.frame(plan),after.frame(scene.draw_plan()));};
 auto bad=instance;bad.model=nullptr;R(!scene.plan_prepared({&bad,1},view,error));retained();bad=instance;bad.object_to_world[0]=std::bit_cast<float>(1u);R(!scene.plan_prepared({&bad,1},view,error));retained();
 WorldView invalid=view;invalid.facing_origin.x=std::bit_cast<float>(0x7fc12345u);R(!scene.plan_prepared({&instance,1},invalid,error));retained();
 std::array<PreparedWorldInstance,3>too_many{instance,instance,instance};R(!scene.plan_prepared(too_many,view,error)&&error==WorldSceneError::input_limit);retained();
 for(long n=0;n<5;++n){fail_after=n;const bool ok=scene.configure_plan(12,96,error);fail_after=-1;R(!ok&&error==WorldSceneError::allocation_failed);retained();}
 // A late facing arithmetic rejection must not publish partially selected data.
 Source tiny;tiny.faces[5].facing.normal={0x1p-126f,0,0};tiny.faces[5].facing.point={1,0,0};tiny.groups={{{0,1},{1,1},{2,4}}};tiny.points[6].x=0;tiny.points[7].x=.25f;tiny.points[8].x=0;PreparedWorldModel late;R(late.assign(tiny.model(),error));PreparedWorldInstance last{&late,identity()};last.object_to_world[0]=.5f;
 R(!scene.plan_prepared({&last,1},view,error)&&error==WorldSceneError::invalid_transform);retained();
 // Frame publication/configuration has an independent lifetime and storage.
 R(scene.configure(6,96,error));scene.set_numeric_mode(WorldNumericMode::native_float);R(scene.update_prepared({&instance,1},view,error));retained();const auto frame=scene.frame();
 R(scene.configure_plan(12,96,error));R(scene.draw_plan().output_triangle_capacity()==0&&scene.frame().triangles.data()==frame.triangles.data());
 // Flat-only owners need a lease too, although no texture points into them.
 std::array<WorldPoint,3>points{{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f}}};WorldFace face;face.indices={0,1,2};
 {PreparedWorldModel flat;R(flat.assign({points,{&face,1}},error));PreparedWorldInstance f{&flat,identity()};R(scene.plan_prepared({&f,1},{identity()},error));}
 R(after.render(scene.draw_plan())&&after.triangles.size()==1);
 // Planning is not a vertex transform/domain or expanded-capacity proof.
 PreparedWorldModel huge;for(auto&p:points)p.x=65536;R(huge.assign({points,{&face,1}},error));PreparedWorldInstance h{&huge,identity()};h.object_to_world[0]=65536;R(scene.plan_prepared({&h,1},{identity()},error));R(!after.render(scene.draw_plan()));
}
}
int main(){parity();lifetime_and_atomicity();std::printf("WORLD_PLAN PASS frames=%u checks=%u ordered_geometry=1 immutable_leases=1 atomic_failure=1 no_plan_allocations=1\n",frames,checks);}
