#include "rally_visual_scene.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
namespace {std::size_t allocations=0;long fail_after=-1;}
void*operator new(std::size_t n){++allocations;if(!fail_after)throw std::bad_alloc();if(fail_after>0)--fail_after;
 if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0;
void require(bool okay,unsigned line){++checks;if(!okay){std::fprintf(stderr,"RALLY_VISUAL FAIL line=%u checks=%u\n",line,checks);std::exit(1);}}
#define REQUIRE(x) require(bool(x),__LINE__)
void put(std::span<std::uint8_t> b,std::size_t p,std::uint32_t x){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(x>>(8*i));}
void putf(std::span<std::uint8_t>b,std::size_t p,float f){put(b,p,std::bit_cast<std::uint32_t>(f));}
std::uint32_t crc(std::span<const std::uint8_t>b){std::uint32_t c=~0u;for(auto x:b){c^=x;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
std::array<std::uint8_t,320> encode(const RallyVisualConfig&c){std::array<std::uint8_t,320>b{};std::memcpy(b.data(),"RLYVIEW1",8);
 put(b,8,1);put(b,12,320);put(b,16,288);put(b,24,c.initial_game_frame);std::size_t p=32;
 const auto words=[&](const auto&v){for(auto x:v){put(b,p,x);p+=4;}};
 words(c.initial_pose.words.position);words(c.initial_pose.words.motion);words(c.initial_pose.words.orientation);words(c.initial_pose.words.attributes);
 put(b,112,c.initial_pose.flags);put(b,116,c.initial_pose.progress);putf(b,120,c.initial_view_state.yaw_history);putf(b,124,c.view.distance_adjustment);
 for(unsigned i=0;i<4;++i){const auto v=c.geometry.wheel_offsets[i];putf(b,128+i*12,v.x);putf(b,132+i*12,v.y);putf(b,136+i*12,v.z);}
 putf(b,176,c.near_z);putf(b,180,c.far_z);put(b,184,c.clear_color);put(b,188,c.asset_count);
 for(unsigned i=0;i<7;++i){put(b,192+i*4,c.role_assets[i]);put(b,220+i*8,c.assets[i].bytes);put(b,224+i*8,c.assets[i].crc32);}
 put(b,20,crc(std::span(b).subspan(32)));return b;
}
RallyVisualConfig config(){RallyVisualConfig c;c.initial_game_frame=17;c.asset_count=1;c.assets[0]={264,0};
 c.geometry.wheel_offsets={AffinePoint3{-.7f,0,.6f},AffinePoint3{.7f,0,.6f},AffinePoint3{-.7f,0,-.6f},AffinePoint3{.7f,0,-.6f}};return c;}
struct Source {
 std::array<WorldPoint,3> points{{{-.5f,0,0},{.5f,0,0},{0,.5f,0}}};
 std::array<WorldFace,1> faces{{{{0,1,2},0xffc02040u,{{{0,0},{8,0},{4,8}}},0}}};
 std::array<std::uint8_t,32> pixels{};Texture texture{pixels.data(),pixels.size(),8,8,9};
 std::array<Material,1> materials{};
 Source(){materials[0].texture=&texture;materials[0].palette.fill(0xffc02040u);}
 WorldModel model(){return {points,faces,{},materials};}
};
std::uint64_t hash(Frame f){std::uint64_t h=1469598103934665603ull;const auto word=[&](std::uint32_t x){h=(h^x)*1099511628211ull;};
 word(f.triangles.size());for(const auto&t:f.triangles){word(t.material->color);for(auto x:t.material->palette)word(x);
  for(const auto&v:t.vertices)for(float x:{v.x,v.y,v.u,v.v,v.q,v.depth})word(std::bit_cast<std::uint32_t>(x));}return h;}
void config_checks(){auto c=config();const auto valid=encode(c);RallyVisualConfig output;RallyVisualError e;
 REQUIRE(decode_rally_visual_config(valid,17,output,e));REQUIRE(output.initial_pose==c.initial_pose);
 const auto preserved=encode(output);
 for(unsigned byte:{0u,8u,12u,16u,20u,28u,276u,319u}){auto b=valid;b[byte]^=1;
  if(byte>=32)put(b,20,crc(std::span(b).subspan(32)));
  REQUIRE(!decode_rally_visual_config(b,17,output,e));REQUIRE(encode(output)==preserved);}
 REQUIRE(!decode_rally_visual_config(valid,18,output,e)&&e==RallyVisualError::frame_mismatch);
 REQUIRE(!decode_rally_visual_config(std::span(valid).first(319),17,output,e));
 for(unsigned field:{120u,124u,128u,176u,180u}){auto b=valid;put(b,field,0x7fc00000u);put(b,20,crc(std::span(b).subspan(32)));
  REQUIRE(!decode_rally_visual_config(b,17,output,e));}
 for(unsigned field:{188u,192u,220u}){auto b=valid;put(b,field,0xffffffffu);put(b,20,crc(std::span(b).subspan(32)));
  REQUIRE(!decode_rally_visual_config(b,17,output,e));}
 c.asset_count=2;c.assets[1]={64,0};REQUIRE(!decode_rally_visual_config(encode(c),17,output,e)); // Unused declaration.
}
bool provider(void* count,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4> input,
              std::span<ps2::TransformVector4> output,std::span<std::uint8_t> accepted)noexcept {
 ++*static_cast<unsigned*>(count);
 for(std::size_t i=0;i<input.size();++i){std::array<float,4> p{input[i].x,input[i].y,input[i].z,input[i].w},w{},q{};
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)w[r]+=a[r+4*k]*p[k];
  for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)q[r]+=b[r+4*k]*w[k];
  output[i]={q[0],q[1],q[2],q[3]};accepted[i]=1;}return true;
}
bool project_provider(void* count,const WorldMatrix&a,const WorldMatrix&b,std::span<const ps2::TransformVector4> input,
              std::span<ps2::TransformVector4> output,std::span<std::uint8_t> accepted,
              std::span<ps2::TransformProjectedVertex> projected)noexcept {
 // Exercise forwarding and replacement independently of the hardware kernel;
 // q=0 deliberately retains the ordinary projection arithmetic in this test.
 for(auto& p:projected)p={};
 return provider(count,a,b,input,output,accepted);
}
void movement(){Source source;const auto model=source.model();const auto c=config();RallyVisualError e;
 RallyVisualScene scene,control;unsigned provider_calls=0;scene.set_transform_project_provider(project_provider,&provider_calls);
 REQUIRE(scene.configure(c,{&model,1},e));REQUIRE(control.configure(c,{&model,1},e));
 auto wrong=c.initial_pose;wrong.progress=1;REQUIRE(!scene.update(wrong,e)&&e==RallyVisualError::initial_pose);REQUIRE(scene.frame().triangles.empty());
 const auto before=allocations;REQUIRE(scene.update(c.initial_pose,e));REQUIRE(control.update(c.initial_pose,e));REQUIRE(allocations==before);
 REQUIRE(provider_calls>0);REQUIRE(scene.frame().triangles.size()==7);REQUIRE(hash(scene.frame())==hash(control.frame()));
 const auto original=hash(scene.frame());const float old_x=scene.frame().triangles[0].vertices[0].x;
 // Independent projected course corner: explicit recovered camera C followed
 // by original logical center/focus; no captured matrices or motion samples.
 auto history=c.initial_view_state;DrivingRenderView camera;REQUIRE(update_driving_render_view(c.initial_pose,c.view,history,camera));
 const auto p=source.points[0];const auto&m=camera.world_to_view;
 const float x=m.columns[0].x*p.x+m.columns[1].x*p.y+m.columns[2].x*p.z+m.translation.x;
 const float y=m.columns[0].y*p.x+m.columns[1].y*p.y+m.columns[2].y*p.z+m.translation.y;
 const float z=m.columns[0].z*p.x+m.columns[1].z*p.y+m.columns[2].z*p.z+m.translation.z;
 const auto&v=scene.frame().triangles[0].vertices[0];
 REQUIRE(std::fabs(v.x-(248+300*x/z))<.0001f);REQUIRE(std::fabs(v.y-(192-300*y/z))<.0001f);
 REQUIRE(v.u==0&&v.v==0);REQUIRE(std::fabs(v.q-1/z)<.000001f);
 auto moved=c.initial_pose;moved.words.position[0]=std::bit_cast<std::uint32_t>(.25f);
 REQUIRE(scene.update(moved,e));REQUIRE(control.update(moved,e));REQUIRE(scene.frame().triangles[0].vertices[0].x<old_x);
 REQUIRE(hash(scene.frame())==hash(control.frame()));REQUIRE(hash(scene.frame())!=original);
 const auto prior=hash(scene.frame());auto invalid=moved;invalid.words.position[0]=0x7fc00000u;
 REQUIRE(!scene.update(invalid,e));REQUIRE(hash(scene.frame())==prior);
 // A numerically valid pose whose view translation exceeds the generic
 // engine domain must not commit its changed camera history either.
 invalid=moved;invalid.words.position[0]=std::bit_cast<std::uint32_t>(100000.f);invalid.words.orientation[1]=std::bit_cast<std::uint32_t>(.2f);
 REQUIRE(!scene.update(invalid,e));REQUIRE(hash(scene.frame())==prior);
 moved.words.orientation[1]=std::bit_cast<std::uint32_t>(.1f);REQUIRE(scene.update(moved,e));REQUIRE(control.update(moved,e));
 REQUIRE(hash(scene.frame())==hash(control.frame()));
 const auto keep=hash(scene.frame());fail_after=0;const bool replaced=scene.configure(c,{&model,1},e);fail_after=-1;
 REQUIRE(!replaced&&e==RallyVisualError::allocation);REQUIRE(hash(scene.frame())==keep);
 const auto reset_allocations=allocations;scene.reset();REQUIRE(allocations==reset_allocations);REQUIRE(scene.frame().triangles.empty());
 REQUIRE(!scene.update(moved,e)&&e==RallyVisualError::initial_pose);REQUIRE(scene.update(c.initial_pose,e));REQUIRE(hash(scene.frame())==original);
 // Prepared ownership protects every published material from source changes.
 source.points.fill({100,100,100});source.pixels.fill(0xff);source.materials[0].palette.fill(0);
 scene.reset();REQUIRE(scene.update(c.initial_pose,e));REQUIRE(hash(scene.frame())==original);
 const auto calls=provider_calls;scene.set_transform_provider(nullptr);scene.reset();
 REQUIRE(scene.update(c.initial_pose,e));REQUIRE(provider_calls==calls);REQUIRE(hash(scene.frame())==original);
 REQUIRE(scene.persistent_bytes()>0);
}
std::vector<std::uint8_t> model_bytes(){std::vector<std::uint8_t>b(264);std::memcpy(b.data(),"RLYTEXM1",8);
 put(b,8,1);put(b,12,64);put(b,16,3);put(b,20,1);put(b,24,1);put(b,28,1);put(b,32,32);put(b,40,200);
 put(b,44,12);put(b,48,40);put(b,52,76);put(b,56,16);
 const Source s;std::size_t p=64;for(auto v:s.points){putf(b,p,v.x);putf(b,p+4,v.y);putf(b,p+8,v.z);p+=12;}
 put(b,p,0);put(b,p+4,1);put(b,p+8,2);putf(b,p+20,8);putf(b,p+28,4);putf(b,p+32,8);p+=40;
 put(b,p,0);put(b,p+4,0xffffffffu);for(unsigned i=0;i<16;++i)put(b,p+12+i*4,0xffc02040u);p+=76;
 put(b,p,8);put(b,p+4,8);put(b,p+8,0);put(b,p+12,32);put(b,36,crc(std::span(b).subspan(64)));return b;}
void write_file(const std::string&path,std::span<const std::uint8_t>b){FILE*f=std::fopen(path.c_str(),"wb");REQUIRE(f);REQUIRE(std::fwrite(b.data(),1,b.size(),f)==b.size());REQUIRE(std::fclose(f)==0);}
void files(const char* directory){auto c=config();auto asset=model_bytes();c.assets[0]={std::uint32_t(asset.size()),crc(asset)};
 const std::string root=directory;const auto asset_path=root+"/model0.bin",config_path=root+"/view.bin";
 write_file(asset_path,asset);write_file(config_path,encode(c));RallyVisualScene scene;RallyVisualError e;
 unsigned calls=0;scene.set_transform_project_provider(project_provider,&calls);
 REQUIRE(scene.load(directory,17,e));REQUIRE(scene.update(c.initial_pose,e));const auto previous=hash(scene.frame());
 REQUIRE(calls>0);
 REQUIRE(!scene.load(directory,18,e)&&e==RallyVisualError::frame_mismatch);REQUIRE(hash(scene.frame())==previous);
 asset.back()^=1;write_file(asset_path,asset);REQUIRE(!scene.load(directory,17,e)&&e==RallyVisualError::asset_identity);REQUIRE(hash(scene.frame())==previous);
 asset.back()^=1;asset.push_back(0);write_file(asset_path,asset);REQUIRE(!scene.load(directory,17,e)&&e==RallyVisualError::asset_identity);
 asset.pop_back();write_file(asset_path,asset);auto bad=encode(c);bad.back()=1;write_file(config_path,bad);
 REQUIRE(!scene.load(directory,17,e)&&e==RallyVisualError::config);REQUIRE(hash(scene.frame())==previous);
}
}
int main(int argc,char**argv){REQUIRE(argc==2);config_checks();movement();files(argv[1]);
 std::printf("RALLY_VISUAL PASS checks=%u native_pose_movement=1 projection=1 reset=1 asset_binding=1 atomic_history=1 no_update_allocations=1\n",checks);}
