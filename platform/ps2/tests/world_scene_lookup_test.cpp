#include "world_scene.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace ps2::native;
namespace {unsigned checks=0,calls=0,largest=0;void check(bool v){++checks;if(!v){std::fprintf(stderr,"WORLD_LOOKUP FAIL %u\n",checks);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
bool batch(void*ctx,const WorldMatrix&,const WorldMatrix&,std::span<const ps2::TransformVector4>in,std::span<ps2::TransformVector4>out,std::span<std::uint8_t>accepted)noexcept{
 ++calls;largest=std::max(largest,unsigned(in.size()));const unsigned mode=*static_cast<unsigned*>(ctx);
 for(unsigned i=0;i<in.size();++i){out[i]=in[i];accepted[i]=(mode==1||(mode==3&&(i&1)))?0:1;}
 if(mode==4)out[0].x=std::bit_cast<float>(0x7fc00000u);
 return mode!=2;}
std::uint64_t hash(Frame f){std::uint64_t h=1469598103934665603ull;auto add=[&](std::uint32_t x){for(unsigned b=0;b<4;++b){h^=(x>>(b*8))&255;h*=1099511628211ull;}};
 add(f.triangles.size());for(const auto&t:f.triangles){add(t.material->color);for(const auto&v:t.vertices)for(float x:{v.x,v.y,v.depth,v.u,v.v,v.q})add(std::bit_cast<std::uint32_t>(x));}return h;}
}
int main(){WorldSceneError e;WorldScene baseline,candidate;check(baseline.configure(64,64,e));check(candidate.configure(64,64,e));
 std::vector<WorldPoint>p(65536,{0,0,.5f});std::vector<WorldFace>f(64);
 // Every requested index hashes to the same bucket; index65535 also exercises
 // the maximum accepted vertex key and its distinct bucket.
 for(unsigned i=0;i<96;++i){const unsigned index=i==95?65535:i*256;p[index]={float(int(i%3)-1)/4,float(i%3==1?1:-1)/4,.5f};}
 for(unsigned i=0;i<64;++i){const unsigned base=(i%32)*3;f[i]={{base*256,(base+1)*256,base==93?65535:(base+2)*256},0xffabcdef};}
 PreparedWorldModel owned;check(owned.assign({p,f},e));PreparedWorldInstance instance{&owned,identity()};WorldView view{identity()};
 unsigned mode=0;candidate.set_transform_provider(batch,&mode);
 for(auto numeric:{WorldNumericMode::reference,WorldNumericMode::native_float})for(mode=0;mode<5;++mode){baseline.set_numeric_mode(numeric);candidate.set_numeric_mode(numeric);
  for(unsigned n=0;n<8;++n){check(baseline.update_prepared({&instance,1},view,e));const auto expected=hash(baseline.frame());
   check(candidate.update_prepared({&instance,1},view,e));check(hash(candidate.frame())==expected);check(candidate.stats().batch_vertices==190);check((candidate.stats().failed_batches!=0)==(mode==2));}}
 check(largest==96);
 const std::array<WorldPoint,3>small{{{-.25f,-.25f,.5f},{.25f,-.25f,.5f},{0,.25f,.5f}}};const std::array<WorldFace,1>face{{{{0,1,2},0xff123456}}};
 std::array<WorldFace,64> repeated;repeated.fill(face[0]);check(owned.assign({small,repeated},e));mode=0;
 check(baseline.update_prepared({&instance,1},view,e));check(candidate.update_prepared({&instance,1},view,e));
 check(hash(baseline.frame())==hash(candidate.frame()));check(candidate.stats().batch_vertices==3&&candidate.stats().batch_calls==1);
 check(owned.assign({small,face},e));mode=0;check(baseline.update_prepared({&instance,1},view,e));const auto expected=hash(baseline.frame());
 for(unsigned n=0;n<70000;++n){check(candidate.update_prepared({&instance,1},view,e));check(hash(candidate.frame())==expected);}
 std::printf("WORLD_LOOKUP PASS checks=%u wrap_updates=70000 largest_batch=%u provider_calls=%u\n",checks,largest,calls);
}
