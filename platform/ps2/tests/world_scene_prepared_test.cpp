#include "world_scene.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
namespace {long fail_after=-1;std::size_t allocations=0;}
void* operator new(std::size_t n){++allocations;if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0;
void check(bool yes,unsigned line){++checks;if(!yes){std::fprintf(stderr,"WORLD_PREPARED FAIL line=%u check=%u\n",line,checks);std::exit(1);}}
#define REQUIRE(x) check(bool(x),__LINE__)
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::array<std::uint32_t,6> words(const Vertex&v){return {std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.depth),std::bit_cast<std::uint32_t>(v.u),std::bit_cast<std::uint32_t>(v.v),std::bit_cast<std::uint32_t>(v.q)};}
void equal(Frame a,Frame b){REQUIRE(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
 for(std::size_t i=0;i<a.triangles.size();++i){REQUIRE(a.triangles[i].material->color==b.triangles[i].material->color);
  for(unsigned c=0;c<3;++c)REQUIRE(words(a.triangles[i].vertices[c])==words(b.triangles[i].vertices[c]));}}
std::array<WorldPoint,6> points{{{-.75f,-.25f,.5f},{.25f,-.25f,.5f},{0,.75f,.5f},{-.25f,.25f,.75f},{.75f,.25f,.75f},{.25f,-.75f,.75f}}};
std::array<WorldFace,2> faces{{{{0,1,2},0xff123456},{{3,4,5},0xffaabbcc}}};
std::array<WorldFaceGroup,2> groups{{{0,1},{1,1}}};
bool provider(void*,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4>in,std::span<ps2::TransformVector4>out,std::span<std::uint8_t>accepted)noexcept{
 for(std::size_t i=0;i<in.size();++i){const std::array<float,4>p{in[i].x,in[i].y,in[i].z,in[i].w};std::array<float,4>w{},q{};
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)w[r]+=a[r+4*c]*p[c];
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)q[r]+=b[r+4*c]*w[c];
  out[i]={q[0],q[1],q[2],q[3]};accepted[i]=1;}return true;}
void parity(){
 WorldScene ordinary,prepared;WorldSceneError a,b;REQUIRE(ordinary.configure(4,32,a));REQUIRE(prepared.configure(4,32,b));
 PreparedWorldModel model;WorldView view{identity()};
 for(unsigned grouped=0;grouped<2;++grouped){WorldModel source{points,faces,grouped?std::span<const WorldFaceGroup>(groups):std::span<const WorldFaceGroup>()};
  REQUIRE(model.assign(source,b));REQUIRE(model.valid());const auto bytes=model.persistent_bytes();REQUIRE(bytes>=points.size()*sizeof(WorldPoint)+faces.size()*sizeof(WorldFace));
  std::array<WorldInstance,2> plain{{{source,identity()},{source,identity()}}};
  std::array<PreparedWorldInstance,2> owned{{{&model,identity()},{&model,identity()}}};
  for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float})for(unsigned use_provider=0;use_provider<2;++use_provider){
   ordinary.set_numeric_mode(mode);prepared.set_numeric_mode(mode);ordinary.set_transform_provider(use_provider?provider:nullptr);prepared.set_transform_provider(use_provider?provider:nullptr);
   for(unsigned n=0;n<97;++n){plain[0].object_to_world=identity();plain[1].object_to_world=identity();
    plain[0].object_to_world[12]=float(int(n%17)-8)/4;plain[0].object_to_world[13]=float(int(n%7)-3)/8;
    plain[1].object_to_world[0]=(n&1)?-1.f:1.f;plain[1].object_to_world[14]=float(int(n%9)-4)/4;
    for(unsigned i=0;i<2;++i)owned[i].object_to_world=plain[i].object_to_world;
    view.world_to_clip=identity();view.world_to_clip[11]=(n&2)?.5f:0;view.world_to_clip[1]=(n&4)?.25f:0;
    const auto before=allocations;const bool x=ordinary.update(plain,view,a),y=prepared.update_prepared(owned,view,b);REQUIRE(allocations==before);REQUIRE(x==y&&a==b);REQUIRE(x);equal(ordinary.frame(),prepared.frame());
    REQUIRE(ordinary.stats().culled_groups==prepared.stats().culled_groups);REQUIRE(ordinary.stats().batch_vertices==prepared.stats().batch_vertices);REQUIRE(model.persistent_bytes()==bytes);++frames;
   }
  }
 }
}
void ownership(){
 WorldScene scene;WorldSceneError error;REQUIRE(scene.configure(4,32,error));WorldView view{identity()};PreparedWorldModel owner;
 REQUIRE(!owner.valid()&&owner.persistent_bytes()==0);PreparedWorldInstance instance{&owner,identity()};
 REQUIRE(!scene.update_prepared({&instance,1},view,error)&&error==WorldSceneError::invalid_model);
 {auto p=points;auto f=faces;auto g=groups;REQUIRE(owner.assign({p,f,g},error));
  p[0].x=std::bit_cast<float>(0x7fc00000u);f[0].argb=0;g[0].face_count=0;
  REQUIRE(scene.update_prepared({&instance,1},view,error));REQUIRE(scene.frame().triangles[0].material->color==faces[0].argb);}
 REQUIRE(scene.update_prepared({&instance,1},view,error));const auto bytes=owner.persistent_bytes();
 const auto expected=words(scene.frame().triangles[0].vertices[0]);
 // Failed assignments retain all owned data, including cached group extrema.
 for(unsigned kind=0;kind<6;++kind){auto p=points;auto f=faces;auto g=groups;
  if(kind==0)p[0].x=std::bit_cast<float>(1u);if(kind==1)f[0].indices[0]=99;if(kind==2)f[0].indices[1]=f[0].indices[0];
  if(kind==3)f[0].argb=0;if(kind==4)g[0].first_face=1;if(kind==5)g[1].face_count=0;
  REQUIRE(!owner.assign({p,f,g},error)&&error==WorldSceneError::invalid_model);REQUIRE(owner.persistent_bytes()==bytes);
  REQUIRE(scene.update_prepared({&instance,1},view,error));REQUIRE(words(scene.frame().triangles[0].vertices[0])==expected);
 }
 for(long fail=0;fail<5;++fail){fail_after=fail;const bool ok=owner.assign({points,faces,groups},error);fail_after=-1;
  REQUIRE(!ok&&error==WorldSceneError::allocation_failed);REQUIRE(owner.persistent_bytes()==bytes);REQUIRE(scene.update_prepared({&instance,1},view,error));REQUIRE(words(scene.frame().triangles[0].vertices[0])==expected);}
 auto changed=points;for(auto&p:changed)p.x+=.125f;REQUIRE(owner.assign({changed,faces,groups},error));REQUIRE(scene.update_prepared({&instance,1},view,error));
 REQUIRE(scene.frame().triangles[0].vertices[0].x==std::bit_cast<float>(expected[0])+31);
 // Successful frame owns material/vertex data after the model itself dies.
 {PreparedWorldModel temporary;REQUIRE(temporary.assign({points,faces},error));instance.model=&temporary;REQUIRE(scene.update_prepared({&instance,1},view,error));}
 REQUIRE(words(scene.frame().triangles[0].vertices[0])==expected&&scene.frame().triangles[0].material->color==faces[0].argb);
}
void rollback(){
 WorldScene scene;WorldSceneError error;WorldView view{identity()};PreparedWorldModel model;REQUIRE(model.assign({points,faces,groups},error));PreparedWorldInstance instance{&model,identity()};
 REQUIRE(!scene.update_prepared({&instance,1},view,error)&&error==WorldSceneError::not_configured);REQUIRE(scene.configure(4,2,error));REQUIRE(scene.update_prepared({&instance,1},view,error));
 const auto frame=scene.frame();const auto vertex=words(frame.triangles[0].vertices[0]);const auto stats=scene.stats();
 const auto retained=[&](){REQUIRE(scene.frame().triangles.data()==frame.triangles.data()&&scene.frame().triangles.size()==frame.triangles.size());REQUIRE(words(scene.frame().triangles[0].vertices[0])==vertex);REQUIRE(scene.stats().output_triangles==stats.output_triangles&&scene.stats().source_faces==stats.source_faces);};
 auto bad=instance;bad.model=nullptr;REQUIRE(!scene.update_prepared({&bad,1},view,error)&&error==WorldSceneError::invalid_model);retained();
 bad=instance;bad.object_to_world[0]=std::bit_cast<float>(1u);REQUIRE(!scene.update_prepared({&bad,1},view,error)&&error==WorldSceneError::invalid_transform);retained();
 auto invalid_view=view;invalid_view.world_to_clip[15]=std::bit_cast<float>(0x7f800000u);REQUIRE(!scene.update_prepared({&instance,1},invalid_view,error));retained();
 std::array<PreparedWorldInstance,2> two{instance,instance};REQUIRE(!scene.update_prepared(two,view,error)&&error==WorldSceneError::output_limit);retained();
 std::array<PreparedWorldInstance,3> three{instance,instance,instance};REQUIRE(!scene.update_prepared(three,view,error)&&error==WorldSceneError::input_limit);retained();
 std::vector<PreparedWorldInstance> many(1025,instance);REQUIRE(!scene.update_prepared(many,view,error)&&error==WorldSceneError::input_limit);retained();
 // Exact per-update transform validation remains necessary even for a culled group.
 auto p=points;for(auto&v:p)v.x=0x1p-100f;PreparedWorldModel tiny;REQUIRE(tiny.assign({p,faces,groups},error));
 WorldScene ordinary;REQUIRE(ordinary.configure(4,32,error));WorldInstance raw{{p,faces,groups},identity()};PreparedWorldInstance owned{&tiny,identity()};
 for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float}){
  ordinary.set_numeric_mode(mode);scene.set_numeric_mode(mode);raw.object_to_world[0]=owned.object_to_world[0]=0x1p-40f;WorldSceneError expected;
  REQUIRE(!ordinary.update({&raw,1},view,expected));REQUIRE(!scene.update_prepared({&owned,1},view,error)&&error==expected);retained();}
 REQUIRE(scene.update_prepared({},view,error)&&scene.frame().triangles.empty());
}
void limits(){
 WorldSceneError error;PreparedWorldModel model;std::vector<WorldPoint>p(65536,points[0]);std::vector<WorldFace>f(32768,faces[0]);
 REQUIRE(model.assign({p,f},error));const auto bytes=model.persistent_bytes();REQUIRE(bytes>=65536*sizeof(WorldPoint)+32768*sizeof(WorldFace));
 p.push_back(points[0]);REQUIRE(!model.assign({p,f},error)&&error==WorldSceneError::input_limit&&model.persistent_bytes()==bytes);p.pop_back();
 f.push_back(faces[0]);REQUIRE(!model.assign({p,f},error)&&error==WorldSceneError::input_limit&&model.persistent_bytes()==bytes);
 // Per-instance vertex counts still accumulate when owners are shared.
 p.resize(40000);f.resize(1);REQUIRE(model.assign({p,f},error));WorldScene scene;REQUIRE(scene.configure(2,2,error));
 std::array<PreparedWorldInstance,2>two{{{&model,identity()},{&model,identity()}}};REQUIRE(!scene.update_prepared(two,{identity()},error)&&error==WorldSceneError::input_limit);
}
}
int main(){parity();ownership();rollback();limits();std::printf("WORLD_PREPARED PASS frames=%u checks=%u immutable_ownership=1 atomic_replacement=1 no_update_allocations=1\n",frames,checks);}
