#include "world_scene.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
namespace {std::size_t allocations=0;long fail_after=-1;}
void* operator new(std::size_t n){++allocations;if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0,largest=0;std::vector<std::uint32_t>*record=nullptr;
void require(bool yes,unsigned line){++checks;if(!yes){std::fprintf(stderr,"WORLD_GATHER FAIL line=%u check=%u frame=%u\n",line,checks,frames);std::exit(1);}}
#define R(x) require(bool(x),__LINE__)
void add(std::uint32_t x){if(record){R(record->size()<record->capacity());record->push_back(x);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
struct Context{unsigned fault=0;};
bool provider(void*opaque,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4>input,std::span<ps2::TransformVector4>output,std::span<std::uint8_t>accepted)noexcept{
 auto&c=*static_cast<Context*>(opaque);largest=std::max(largest,unsigned(input.size()));add(0x11111111);add(input.size());
 for(float f:a)add(std::bit_cast<std::uint32_t>(f));
 for(float f:b)add(std::bit_cast<std::uint32_t>(f));
 for(std::size_t i=0;i<input.size();++i){const std::array<float,4>p{input[i].x,input[i].y,input[i].z,input[i].w};std::array<float,4>w{},q{};
  for(float f:p)add(std::bit_cast<std::uint32_t>(f));
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)w[r]+=a[r+k*4]*p[k];
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)q[r]+=b[r+k*4]*w[k];
  output[i]={q[0],q[1],q[2],q[3]};accepted[i]=1;
  if(c.fault==1)accepted[i]=std::uint8_t(i&1);
  if(c.fault==2)output[i]={0,0,0,0};
  if(c.fault==3){switch(i%4){case 0:output[i].x=std::bit_cast<float>(0x7fc12345u);break;case 1:output[i].x=std::bit_cast<float>(1u);break;case 2:output[i].w=0;break;default:accepted[i]=2;}}
 }
 return c.fault!=2;
}
bool projected(void*c,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4>in,std::span<ps2::TransformVector4>out,std::span<std::uint8_t>accepted,std::span<ps2::TransformProjectedVertex>p)noexcept{
 const bool ok=provider(c,a,b,in,out,accepted);for(std::size_t i=0;i<in.size();++i){p[i]={};const auto&v=out[i];if(v.w>=clip_min_w&&v.x>=-v.w&&v.x<=v.w&&v.y>=-v.w&&v.y<=v.w&&v.z>=0&&v.z<=v.w){const float q=1/v.w;p[i]={(v.x*q+1)*248,(1-v.y*q)*192,1-v.z*q,q};}}return ok;
}
std::array<std::uint32_t,6> words(Vertex v){return {std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.depth),std::bit_cast<std::uint32_t>(v.u),std::bit_cast<std::uint32_t>(v.v),std::bit_cast<std::uint32_t>(v.q)};}
void equal(Frame a,Frame b){R(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());for(std::size_t i=0;i<a.triangles.size();++i){R(a.triangles[i].material->color==b.triangles[i].material->color);for(unsigned k=0;k<3;++k)R(words(a.triangles[i].vertices[k])==words(b.triangles[i].vertices[k]));}}
void stats(const WorldSceneStats&a,const WorldSceneStats&b){R(a.instances==b.instances&&a.vertices==b.vertices&&a.source_faces==b.source_faces&&a.output_triangles==b.output_triangles&&a.groups==b.groups&&a.culled_groups==b.culled_groups&&a.transformed_vertices==b.transformed_vertices&&a.vertex_cache_hits==b.vertex_cache_hits&&a.batch_calls==b.batch_calls&&a.batch_vertices==b.batch_vertices&&a.accepted_batch_vertices==b.accepted_batch_vertices&&a.failed_batches==b.failed_batches&&a.clipped_batch_fallbacks==b.clipped_batch_fallbacks&&a.provider_triangle_corners==b.provider_triangle_corners&&a.provider_projected_vertices==b.provider_projected_vertices&&a.backface_rejected_faces==b.backface_rejected_faces);}
void run(){
 // Low eight index bits collide; index65535 is included. All source points
 // used by a batch are distinct, so provider input words expose its order.
 std::vector<WorldPoint>points(65536,{0,0,.5f});std::vector<WorldFace>faces;std::vector<WorldFaceGroup>groups;
 // Exercise both sides of the32-face window and repeated quad-fan corners.
 for(unsigned n:{1u,31u,32u,33u,65u}){const unsigned begin=faces.size();for(unsigned f=0;f<n;++f){WorldFace face;face.argb=0xff102030u+faces.size();
  for(unsigned k=0;k<3;++k){const unsigned corner=(faces.size()*3+k)%256;const auto index=(corner*256+255)&65535;face.indices[k]=index;points[index]={float(int(corner%16)-8)/16,float(int(corner/16)-8)/16,.5f};face.uv[k]={float(corner),float(k)};}
  face.facing={{1,0,0},{float(f%32)/32,0,0},true};faces.push_back(face);}groups.push_back({begin,n});}
 std::vector<std::uint32_t> events[2];for(auto&v:events)v.reserve(20000);
 for(unsigned topology=0;topology<2;++topology){auto f=faces;if(topology)for(std::size_t i=1;i<f.size();++i)if(i%3)f[i].indices=f[i-1].indices;
 PreparedWorldModel owner;WorldSceneError error;R(owner.assign({points,f,groups},error));PreparedWorldInstance instance{&owner,identity()};WorldInstance ordinary{{points,f,groups},identity()};
 for(bool cache:{false,true}){WorldScene scene[2];for(auto&s:scene)R(s.configure(faces.size(),2048,error,cache));Context context;
 for(auto mode:{WorldNumericMode::reference,WorldNumericMode::native_float})for(unsigned callback=0;callback<2;++callback)for(unsigned fault=0;fault<4;++fault){
  context.fault=fault;for(auto&s:scene){s.set_numeric_mode(mode);if(callback)s.set_transform_project_provider(projected,&context);else s.set_transform_provider(provider,&context);}
  for(unsigned start=0;start<33;++start){WorldView view{identity()};view.use_facing=true;view.facing_origin.x=float(start)/32;
   if(start==32)view.world_to_clip[12]=4; // Entire groups culled, no plan/provider use.
   for(unsigned v=0;v<2;++v){record=&events[v];events[v].clear();const auto count=allocations;
    const bool ok=v?scene[v].update_prepared({&instance,1},view,error):scene[v].update({&ordinary,1},view,error);
    R(ok);R(allocations==count);record=nullptr;}
   R(events[0]==events[1]);equal(scene[0].frame(),scene[1].frame());stats(scene[0].stats(),scene[1].stats());++frames;
  }
 }
 // Invalid transforms and output failure leave the previous frame unchanged.
 const auto previous=scene[1].frame();const auto count=previous.triangles.size();auto bad=instance;bad.object_to_world[0]=std::bit_cast<float>(1u);
 R(!scene[1].update_prepared({&bad,1},{identity()},error));R(scene[1].frame().triangles.data()==previous.triangles.data()&&scene[1].frame().triangles.size()==count);
 }
 }
 R(largest==96);
 // Ordinary mutable data remains observable; the prepared owner retains its
 // snapshot until a successful replacement publishes the changed topology.
 points={{{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f}}};faces.resize(1);faces[0].indices={0,1,2};faces[0].facing.enabled=false;
 WorldInstance raw{{points,faces},identity()};WorldSceneError error;
 // Every fallible snapshot allocation is rejected in turn; each failure
 // retains the old immutable data. No model generation is consumed on failure.
 PreparedWorldModel owner;R(owner.assign(raw.model,error));WorldScene owned;R(owned.configure(1,8,error));PreparedWorldInstance pi{&owner,identity()};R(owned.update_prepared({&pi,1},{identity()},error));const auto previous=words(owned.frame().triangles[0].vertices[0]);const auto bytes=owner.persistent_bytes();auto altered=points;altered[0].x+=.125f;unsigned failures=0;
 for(long n=0;n<20;++n){fail_after=n;const bool ok=owner.assign({altered,faces},error);fail_after=-1;if(ok)break;++failures;R(error==WorldSceneError::allocation_failed&&owner.persistent_bytes()==bytes);R(owned.update_prepared({&pi,1},{identity()},error));R(words(owned.frame().triangles[0].vertices[0])==previous);}
 R(failures>0&&failures<20);R(owned.update_prepared({&pi,1},{identity()},error));R(words(owned.frame().triangles[0].vertices[0])!=previous);
 // Source mutation cannot invalidate a prepared gather plan. Reassignment
 // changes its owned topology; failure later in update keeps the prior frame.
 const auto preserved=words(owned.frame().triangles[0].vertices[0]);altered[0].x+=.125f;
 R(owned.update_prepared({&pi,1},{identity()},error));R(words(owned.frame().triangles[0].vertices[0])==preserved);
 WorldScene mutable_scene;R(mutable_scene.configure(1,8,error));WorldInstance changed{{altered,faces},identity()};
 R(mutable_scene.update({&changed,1},{identity()},error));R(words(mutable_scene.frame().triangles[0].vertices[0])!=preserved);
 R(owner.assign(changed.model,error));R(owned.update_prepared({&pi,1},{identity()},error));equal(owned.frame(),mutable_scene.frame());
 R(owned.configure(2,1,error));R(owned.update_prepared({&pi,1},{identity()},error));const auto retained=owned.frame();const auto retained_vertex=words(retained.triangles[0].vertices[0]);
 const std::array<PreparedWorldInstance,2>two{pi,pi};R(!owned.update_prepared(two,{identity()},error)&&error==WorldSceneError::output_limit);
 R(owned.frame().triangles.data()==retained.triangles.data()&&words(owned.frame().triangles[0].vertices[0])==retained_vertex);
 // Failed textured assignments cannot consume immutable generation IDs.
 std::array<std::uint8_t,32>pixels{};Texture texture{pixels.data(),pixels.size(),8,8,0};Material material;material.texture=&texture;material.palette.fill(0xff123456u);auto textured_face=faces[0];textured_face.material_index=0;
 WorldModel textured{points,{&textured_face,1},{},{&material,1}};PreparedWorldModel textured_owner;R(textured_owner.assign(textured,error));PreparedWorldInstance ti{&textured_owner,identity()};R(owned.update_prepared({&ti,1},{identity()},error));const auto initial_generation=owned.frame().triangles[0].material->texture->generation;unsigned texture_failures=0;
 for(long n=0;n<32;++n){fail_after=n;const bool ok=textured_owner.assign(textured,error);fail_after=-1;if(ok)break;++texture_failures;R(error==WorldSceneError::allocation_failed);R(owned.update_prepared({&ti,1},{identity()},error));R(owned.frame().triangles[0].material->texture->generation==initial_generation);}
 R(texture_failures>0&&texture_failures<32);R(owned.update_prepared({&ti,1},{identity()},error));R(owned.frame().triangles[0].material->texture->generation==initial_generation+1);
 std::vector<WorldPoint>maxpoints(65536,{0,0,.5f});std::vector<WorldFace>maxfaces(32768,faces[0]);maxfaces[0].indices={65533,65534,65535};
 R(owner.assign({maxpoints,maxfaces},error));const auto maxbytes=owner.persistent_bytes();maxfaces.push_back(faces[0]);R(!owner.assign({maxpoints,maxfaces},error)&&error==WorldSceneError::input_limit&&owner.persistent_bytes()==maxbytes);
 std::printf("WORLD_GATHER PASS frames=%u checks=%u largest=%u allocation_failures=%u prepared_mutable_parity=1 no_update_allocations=1\n",frames,checks,largest,failures);
}
}
int main(){run();}
