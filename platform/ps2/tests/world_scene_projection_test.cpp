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
}
int main(){
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
