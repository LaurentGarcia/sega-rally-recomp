#include "world_scene.h"
#include "native_geometry_float.h"
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
void check(bool v,unsigned line){++checks;if(!v){std::fprintf(stderr,"WORLD_MATERIAL FAIL line=%u check=%u\n",line,checks);std::exit(1);}}
#define REQUIRE(x) check(bool(x),__LINE__)
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::uint64_t hash(Frame f){std::uint64_t h=1469598103934665603ull;auto add=[&](std::uint32_t w){for(unsigned b=0;b<4;++b){h^=(w>>(8*b))&255;h*=1099511628211ull;}};
 add(f.triangles.size());for(const auto& t:f.triangles){const auto&m=*t.material;add(m.color);add(m.cutout);add(m.checker);add(m.texture!=nullptr);
  if(m.texture){add(m.texture->width);add(m.texture->height);for(auto c:m.palette)add(c);for(std::size_t i=0;i<std::size_t(m.texture->width)*m.texture->height/2;++i)add(m.texture->indices[i]);}
  for(const auto& v:t.vertices)for(float x:{v.x,v.y,v.depth,v.u,v.v,v.q})add(std::bit_cast<std::uint32_t>(x));}return h;}
struct Source {
 std::array<std::uint8_t,32> pixels{};
 Texture texture{pixels.data(),pixels.size(),8,8,77};
 std::array<Material,3> materials{};
 std::array<WorldPoint,5> points{{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f},{2,.5f,.5f},{0,2,.5f}}};
 std::array<WorldFace,4> faces{};
 Source(){for(unsigned i=0;i<pixels.size();++i)pixels[i]=std::uint8_t(i*7);
  for(unsigned i=0;i<2;++i){materials[i].texture=&texture;materials[i].color=0xff123456+i;
   for(unsigned j=0;j<16;++j)materials[i].palette[j]=0xff000000u+(i<<16)+(j<<8)+j;}
  materials[1].cutout=true;materials[1].checker=true;materials[1].palette[15]=0;
  materials[2].color=0xffabcdef;
  faces[0]={{0,1,2},0xffffffff,{{{0,0},{8,0},{4,8}}},0};
  faces[1]={{0,1,2},0xffffffff,{{{16,16},{24,16},{20,24}}},1};
  faces[2]={{0,2,3},0xffffffff,{{{0,0},{0,8},{24,8}}},0};
  faces[3]={{0,4,1},0xffffffff,{{{0,0},{4,24},{8,0}}},1};
 }
 WorldModel model(){return {points,faces,{},materials};}
};
bool provider(void* c,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4> in,std::span<ps2::TransformVector4> out,std::span<std::uint8_t> accepted)noexcept{
 const auto mode=*static_cast<unsigned*>(c);
 for(std::size_t i=0;i<in.size();++i){std::array<float,4> p{in[i].x,in[i].y,in[i].z,in[i].w},w{},q{};
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)w[r]+=a[r+4*k]*p[k];
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)q[r]+=b[r+4*k]*w[k];
  out[i]={q[0],q[1],q[2],q[3]};accepted[i]=mode==1?0:1;}
 return mode!=2;
}
void parity(){Source s;WorldScene cached,uncached,owned;WorldSceneError error;
 REQUIRE(cached.configure(4,32,error));REQUIRE(uncached.configure(4,32,error,false));REQUIRE(owned.configure(4,32,error));
 PreparedWorldModel prepared;REQUIRE(prepared.assign(s.model(),error));WorldInstance plain{s.model(),identity()};PreparedWorldInstance pi{&prepared,identity()};WorldView view{identity()};unsigned provider_mode=0;
 for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float})for(unsigned p=0;p<4;++p){
  cached.set_numeric_mode(mode);uncached.set_numeric_mode(mode);owned.set_numeric_mode(mode);provider_mode=p-1;
  for(auto* scene:{&cached,&uncached,&owned})scene->set_transform_provider(p?provider:nullptr,&provider_mode);
  for(unsigned n=0;n<33;++n){plain.object_to_world=identity();plain.object_to_world[12]=float(int(n%9)-4)/4;pi.object_to_world=plain.object_to_world;
   view.world_to_clip=identity();view.world_to_clip[11]=(n&1)?.5f:0;
   const auto before=allocations;REQUIRE(cached.update({&plain,1},view,error));REQUIRE(uncached.update({&plain,1},view,error));REQUIRE(owned.update_prepared({&pi,1},view,error));REQUIRE(allocations==before);
   REQUIRE(hash(cached.frame())==hash(uncached.frame()));REQUIRE(hash(cached.frame())==hash(owned.frame()));++frames;
  }
 }
 // Independent clip calls verify both UV seams and newly clipped UVs; using
 // only cached/uncached comparison could conceal a shared attribute omission.
 plain.object_to_world=identity();view.world_to_clip=identity();cached.set_transform_provider(nullptr);owned.set_transform_provider(nullptr);
 for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float}){
  cached.set_numeric_mode(mode);REQUIRE(cached.update({&plain,1},view,error));std::vector<Triangle> direct;
  for(const auto& face:s.faces){ClipTriangle in;in.material=&s.materials[face.material_index];
   for(unsigned i=0;i<3;++i){const auto v=s.points[face.indices[i]];in.vertices[i]={v.x,v.y,v.z,1,face.uv[i].u,face.uv[i].v};}
   std::array<Triangle,8> result;const auto r=mode==WorldNumericMode::reference?clip_project_triangle(in,result):clip_project_triangle_float(in,result);
   REQUIRE(r.status==ClipStatus::success);direct.insert(direct.end(),result.begin(),result.begin()+r.count);
  }
  REQUIRE(hash(cached.frame())==hash({direct}));REQUIRE(cached.frame().triangles[0].vertices[0].u==0);REQUIRE(cached.frame().triangles[1].vertices[0].u==16);
 }
 // Returning to the sentinel flat path must clear texture/cutout/checker from
 // a reused material bank, while ordinary material tables may be discarded.
 for(auto& face:s.faces){face.material_index=world_flat_material;face.argb=0xff445566;}
 for(unsigned n=0;n<3;++n){REQUIRE(cached.update({&plain,1},view,error));for(const auto& t:cached.frame().triangles){REQUIRE(!t.material->texture&&!t.material->cutout&&!t.material->checker);REQUIRE(t.material->color==0xff445566);}}
}
void ownership(){WorldScene scene;WorldSceneError error;REQUIRE(scene.configure(4,32,error));WorldView view{identity()};auto owner=std::make_unique<PreparedWorldModel>();
 std::uint64_t expected=0,first_generation=0;
 {Source source;REQUIRE(owner->assign(source.model(),error));PreparedWorldInstance pi{owner.get(),identity()};REQUIRE(scene.update_prepared({&pi,1},view,error));expected=hash(scene.frame());
  const auto* t=scene.frame().triangles[0].material->texture;REQUIRE(t&&t->indices!=source.pixels.data()&&t->bytes==32);first_generation=t->generation;
  REQUIRE((first_generation>>63)==1);
  REQUIRE(t==scene.frame().triangles[1].material->texture);source.pixels.fill(0);source.materials[0].palette.fill(0);source.faces[0].uv[0]={42,42};
  REQUIRE(hash(scene.frame())==expected);REQUIRE(scene.update_prepared({&pi,1},view,error));REQUIRE(hash(scene.frame())==expected);
 }
 Source replacement;replacement.pixels.fill(0x11);REQUIRE(owner->assign(replacement.model(),error));REQUIRE(hash(scene.frame())==expected);
 PreparedWorldInstance pi{owner.get(),identity()};REQUIRE(scene.update_prepared({&pi,1},view,error));REQUIRE(hash(scene.frame())!=expected);expected=hash(scene.frame());
 REQUIRE(scene.frame().triangles[0].material->texture->generation!=first_generation);owner.reset();REQUIRE(hash(scene.frame())==expected);
 PreparedWorldInstance invalid{};REQUIRE(!scene.update_prepared({&invalid,1},view,error));REQUIRE(hash(scene.frame())==expected);
}
void rejection(){Source s;PreparedWorldModel owner;WorldSceneError error;REQUIRE(owner.assign(s.model(),error));WorldScene scene;REQUIRE(scene.configure(4,32,error));PreparedWorldInstance pi{&owner,identity()};WorldView view{identity()};
 REQUIRE(scene.update_prepared({&pi,1},view,error));const auto old=hash(scene.frame());
 auto rejects=[&]{REQUIRE(!owner.assign(s.model(),error));REQUIRE(hash(scene.frame())==old);REQUIRE(scene.update_prepared({&pi,1},view,error));REQUIRE(hash(scene.frame())==old);};
 for(auto bits:{1u,0x80000001u,0x7f800000u,0x7fc00000u,0x47800001u}){s.faces[0].uv[0].u=std::bit_cast<float>(bits);rejects();}s.faces[0].uv[0].u=0;
 s.faces[0].material_index=3;rejects();s.faces[0].material_index=0;
 s.texture.width=7;rejects();s.texture.width=8;s.texture.bytes=31;rejects();s.texture.bytes=32;
 s.texture.indices=nullptr;rejects();s.texture.indices=s.pixels.data();s.materials[0].palette[0]=0x7f010203;rejects();s.materials[0].palette[0]=0xff000000;
 s.materials[2].color=0x80000000;rejects();s.materials[2].color=0xffabcdef;
 Source other;other.pixels.fill(0x33);unsigned failed=0;
 for(long n=0;n<64;++n){fail_after=n;const bool ok=owner.assign(other.model(),error);fail_after=-1;
  REQUIRE(hash(scene.frame())==old);if(ok)break;REQUIRE(error==WorldSceneError::allocation_failed);++failed;
  REQUIRE(scene.update_prepared({&pi,1},view,error));REQUIRE(hash(scene.frame())==old);
 }
 REQUIRE(failed>5&&failed<64);REQUIRE(scene.update_prepared({&pi,1},view,error));REQUIRE(hash(scene.frame())!=old);
 WorldScene limited;REQUIRE(limited.configure(4,1,error));const auto empty=hash(limited.frame());
 REQUIRE(!limited.update_prepared({&pi,1},view,error)&&error==WorldSceneError::output_limit);REQUIRE(hash(limited.frame())==empty);
 // Unique descriptor and copied-byte caps are independent. Unused material
 // entries do not create copied textures; shared descriptors deduplicate.
 std::vector<std::uint8_t> pixels(32768);std::vector<Texture> textures(1025);
 std::vector<Material> materials(1025);std::vector<WorldFace> faces(1025,s.faces[0]);
 for(unsigned i=0;i<textures.size();++i){textures[i]={pixels.data(),pixels.size(),8,8,0};materials[i].texture=&textures[i];materials[i].palette.fill(0xff112233);faces[i].material_index=i;}
 REQUIRE(!owner.assign({s.points,faces,{},materials},error)&&error==WorldSceneError::input_limit);
 faces.resize(129);materials.resize(129);for(unsigned i=0;i<129;++i)textures[i].width=textures[i].height=256;
 REQUIRE(!owner.assign({s.points,faces,{},materials},error)&&error==WorldSceneError::input_limit);
 for(auto& m:materials)m.texture=&textures[0];REQUIRE(owner.assign({s.points,faces,{},materials},error));
 REQUIRE(owner.persistent_bytes()<200000);
}
}
int main(){parity();ownership();rejection();std::printf("WORLD_MATERIAL PASS frames=%u checks=%u seams=1 immutable_assets=1 atomic_failure=1 no_update_allocations=1\n",frames,checks);}
