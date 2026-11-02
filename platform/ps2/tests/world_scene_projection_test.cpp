#include "world_scene.h"
#include "native_geometry_float_internal.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace ps2::native;
namespace {
unsigned checks=0,frames=0;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"WORLD_PROJECTION FAIL %u\n",checks);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::uint64_t hash(Frame frame){std::uint64_t h=1469598103934665603ull;
 auto add=[&](std::uint32_t word){for(unsigned i=0;i<4;++i){h^=(word>>(8*i))&255;h*=1099511628211ull;}};
 add(frame.triangles.size());for(const auto& t:frame.triangles){add(t.material->color);
  for(const auto& v:t.vertices)for(float x:{v.x,v.y,v.u,v.v,v.q,v.depth})add(std::bit_cast<std::uint32_t>(x));}return h;}
bool provider(void* context,const WorldMatrix&,const WorldMatrix&,std::span<const ps2::TransformVector4> in,
              std::span<ps2::TransformVector4> out,std::span<std::uint8_t> accepted) noexcept {
 const auto mode=*static_cast<unsigned*>(context);
 for(unsigned i=0;i<in.size();++i){out[i]=in[i];accepted[i]=mode==1||(mode==3&&(i&1))?0:1;}
 if(mode==4)for(auto& v:out)v.x=std::bit_cast<float>(0x7fc00000u);
 return mode!=2;
}
struct ProjectContext {unsigned mode=0,calls=0;float offset=0;};
bool project_provider(void* context,const WorldMatrix&,const WorldMatrix&,std::span<const ps2::TransformVector4> in,
              std::span<ps2::TransformVector4> out,std::span<std::uint8_t> accepted,
              std::span<ps2::TransformProjectedVertex> projected) noexcept {
 auto& c=*static_cast<ProjectContext*>(context);++c.calls;
 for(unsigned i=0;i<in.size();++i){out[i]=in[i];accepted[i]=(c.mode==3&&(i&1))?0:1;
  if(c.mode==4)out[i].x=std::bit_cast<float>(0x7fc00000u);
  if(c.mode==8)continue; // Deliberately leave the old projection untouched.
  Vertex v;const auto& p=in[i];
  if(detail::project_inside_float({p.x,p.y,p.z,p.w,0,0},v))
   projected[i]={v.x+c.offset,v.y,v.depth,v.q};
  else projected[i]={};
  switch(c.mode){
   case 1:projected[i].q=0;break;
   case 2:projected[i]={-500,-500,-500,-500};break;
   case 5:projected[i].x=std::bit_cast<float>(0x7fc00000u);break;
   case 6:projected[i].y=float(frame_height)+1;break;
   case 7:projected[i].depth=std::bit_cast<float>(1u);break;
   case 9:projected[i].q=-1;break;
   case 10:projected[i].depth=2;break;
   case 11:if(!projected[i].q)projected[i]={20,20,.5f,1};break;
   default:break;
  }
 }
 return c.mode!=2;
}
void projection_continuation(){
 WorldScene scalar,candidate,uncached;WorldSceneError error;
 check(scalar.configure(16,128,error));check(candidate.configure(16,128,error));
 check(uncached.configure(16,128,error,false));
 std::vector<WorldPoint> points(130,{0,0,.5f});
 points[0]={-.5f,-.5f,.5f};points[64]={.5f,-.5f,.5f};points[1]={0,.5f,.5f};points[65]={.5f,.5f,.5f};
 std::array<std::uint8_t,32> pixels{};Texture texture{pixels.data(),pixels.size(),8,8,17};
 Material material;material.texture=&texture;material.palette.fill(0xff123456);
 std::array<WorldFace,16> faces;
 for(unsigned i=0;i<faces.size();++i){faces[i].indices=(i&1)?std::array<std::uint32_t,3>{1,65,0}:std::array<std::uint32_t,3>{0,64,1};
  faces[i].material_index=0;faces[i].uv={WorldUV{float(i),1},WorldUV{2,float(i)},WorldUV{3,4}};
 }
 WorldInstance instance{{points,faces,{},std::span(&material,1)},identity()};WorldView view{identity()};
 ProjectContext context;candidate.set_transform_project_provider(project_provider,&context);
 uncached.set_transform_project_provider(project_provider,&context);
 for(unsigned iteration=0;iteration<300;++iteration){
  context.mode=iteration%12;const auto numeric=iteration%13==0?WorldNumericMode::reference:WorldNumericMode::native_float;
  scalar.set_numeric_mode(numeric);candidate.set_numeric_mode(numeric);uncached.set_numeric_mode(numeric);
  points[64].x=iteration%3==0?1.25f:.5f;points[0].x=iteration%7==0?-1.f:-.5f;
  const auto calls=context.calls;
  check(scalar.update({&instance,1},view,error));check(candidate.update({&instance,1},view,error));
  check(uncached.update({&instance,1},view,error));
  check(hash(scalar.frame())==hash(candidate.frame()));check(hash(scalar.frame())==hash(uncached.frame()));
  if(numeric==WorldNumericMode::reference)check(context.calls==calls);
  if(context.mode==8)check(candidate.stats().provider_projected_vertices==0);
 }
 // A small permitted offset proves the supplied projection is consumed,
 // including after configure(), while UV seams stay per-face.
 points[0].x=-.5f;points[64].x=.5f;context.mode=0;context.offset=1.f/128;
 scalar.set_numeric_mode(WorldNumericMode::native_float);candidate.set_numeric_mode(WorldNumericMode::native_float);
 check(candidate.configure(16,128,error));check(scalar.update({&instance,1},view,error));
 check(candidate.update({&instance,1},view,error));check(candidate.stats().provider_projected_vertices>0);
 const auto reference=scalar.frame(),actual=candidate.frame();check(reference.triangles.size()==actual.triangles.size());
 for(unsigned i=0;i<reference.triangles.size();++i)for(unsigned c=0;c<3;++c){
  const auto a=reference.triangles[i].vertices[c],b=actual.triangles[i].vertices[c];
  check(b.x==a.x+context.offset&&b.y==a.y&&b.u==a.u&&b.v==a.v&&b.q==a.q&&b.depth==a.depth);
 }
 PreparedWorldModel model;check(model.assign(instance.model,error));PreparedWorldInstance prepared{&model,identity()};
 const auto prior=hash(candidate.frame());check(candidate.update_prepared({&prepared,1},view,error));
 check(hash(candidate.frame())==prior);
 const auto calls=context.calls;candidate.set_transform_provider(nullptr);
 check(candidate.update({&instance,1},view,error));check(context.calls==calls);
 check(hash(candidate.frame())==hash(reference));check(candidate.stats().provider_projected_vertices==0);
 candidate.set_transform_project_provider(project_provider,&context);
 check(candidate.configure(16,1,error));const auto empty=hash(candidate.frame());
 check(!candidate.update({&instance,1},view,error)&&error==WorldSceneError::output_limit);
 check(hash(candidate.frame())==empty);
 std::puts("WORLD_PROJECTION CONTINUATION PASS cases=300 invalid_output_fallback=1 seams=1 reference_isolation=1");
}
}
int main(){
 projection_continuation();
 WorldSceneError error;WorldScene uncached,cached;
 check(uncached.configure(16,128,error,false));check(cached.configure(16,128,error));
 std::vector<WorldPoint> points(130,{0,0,.5f});
 points[0]={-.5f,-.5f,.5f};points[64]={.5f,-.5f,.5f};points[1]={0,.5f,.5f};
 points[65]={.5f,.5f,.5f};points[128]={-.5f,.5f,.5f};
 std::array<WorldFace,16> faces;
 for(unsigned i=0;i<faces.size();++i)faces[i]={(i&1)?std::array<std::uint32_t,3>{1,65,0}:std::array<std::uint32_t,3>{0,64,1},0xff102030u+i};
 WorldInstance instance{{points,faces},identity()};WorldView view{identity()};unsigned mode=0;
 uncached.set_transform_provider(provider,&mode);cached.set_transform_provider(provider,&mode);
 // Colliding entries must not overwrite already copied corners. Mutating
 // source geometry, mode and provider status must never reuse an old projection.
 for(unsigned iteration=0;iteration<1000;++iteration){mode=iteration%5;
  const auto numeric=(iteration%7==0)?WorldNumericMode::reference:WorldNumericMode::native_float;
  uncached.set_numeric_mode(numeric);cached.set_numeric_mode(numeric);
  points[64].x=iteration%3==0?1.25f:.5f;
  points[0].x=iteration%11==0?-1.0f:-.5f;
  points[1].y=iteration%13==0?-0.0f:.5f;
  check(uncached.update({&instance,1},view,error));check(cached.update({&instance,1},view,error));
  check(hash(uncached.frame())==hash(cached.frame()));++frames;
 }
 const auto old=hash(cached.frame());points[128].x=std::bit_cast<float>(1u);
 check(!cached.update({&instance,1},view,error)&&error==WorldSceneError::invalid_model);check(hash(cached.frame())==old);
 points[128].x=-.5f;view.world_to_clip[0]=std::bit_cast<float>(0x7f800000u);
 check(!cached.update({&instance,1},view,error)&&error==WorldSceneError::invalid_transform);check(hash(cached.frame())==old);
 view.world_to_clip=identity();WorldScene limited;check(limited.configure(16,1,error));limited.set_numeric_mode(WorldNumericMode::native_float);
 const auto empty=hash(limited.frame());check(!limited.update({&instance,1},view,error)&&error==WorldSceneError::output_limit);check(hash(limited.frame())==empty);

 Material material;ClipTriangle input{{ClipVertex{-.5f,-.5f,.5f,1,0,0},ClipVertex{.5f,-.5f,.5f,1,0,0},ClipVertex{0,.5f,.5f,1,0,0}},&material};
 std::array<Vertex,3> projected;
 for(unsigned i=0;i<3;++i)check(detail::project_inside_float(input.vertices[i],projected[i]));
 std::array<Triangle,8> expected{},actual{};
 check(clip_project_triangle_float(input,expected).count==1);
 const auto result=detail::emit_projected_triangle(projected,&material,actual);check(result.count==1);
 check(hash({std::span(expected).first(1)})==hash({std::span(actual).first(1)}));
 check(detail::emit_projected_triangle(projected,&material,{}).status==ClipStatus::insufficient_capacity);
 const auto saved=hash({std::span(actual).first(1)});
 check(detail::emit_projected_triangle(projected,nullptr,actual).status==ClipStatus::invalid_material);
 check(hash({std::span(actual).first(1)})==saved);
 projected[2]=projected[1];check(detail::emit_projected_triangle(projected,&material,{}).count==0);
 for(const auto bad:{ClipVertex{2,0,.5f,1,0,0},ClipVertex{0,0,.5f,0,0,0},ClipVertex{0,0,-.5f,1,0,0},ClipVertex{std::bit_cast<float>(1u),0,.5f,1,0,0}}){
  Vertex ignored;check(!detail::project_inside_float(bad,ignored));}
 std::printf("WORLD_PROJECTION PASS frames=%u checks=%u collision_and_failure=1 atomic_capacity=1\n",frames,checks);
}
