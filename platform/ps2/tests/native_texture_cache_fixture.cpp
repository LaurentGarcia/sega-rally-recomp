// Original GS page-cache fixture. No game assets, capture or guest runtime.
#include "native_display.h"
#include <dma_registers.h>
#include <kernel.h>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using namespace ps2::native;
struct Asset {std::vector<std::uint8_t> pixels;Texture texture;Material material;unsigned seed=0;};
std::array<std::uint32_t,16> palette(){std::array<std::uint32_t,16> p{};
 for(unsigned i=0;i<16;++i)p[i]=0xff000000u|((20+13*i)<<16)|((230-11*i)<<8)|(35+9*i);return p;}
unsigned texel(unsigned x,unsigned y,unsigned seed){return (x/16+y/8+seed)%16;}
void fill(Asset&a){for(unsigned y=0;y<a.texture.height;++y)for(unsigned x=0;x<a.texture.width;x+=2)
 a.pixels[(y*a.texture.width+x)/2]=std::uint8_t(texel(x,y,a.seed)|(texel(x+1,y,a.seed)<<4));}
void initialize(Asset&a,unsigned width,unsigned height,unsigned seed){a.seed=seed;a.pixels.resize(width*height/2);
 a.texture={a.pixels.data(),a.pixels.size(),std::uint16_t(width),std::uint16_t(height),seed+1};
 a.material.texture=&a.texture;a.material.palette=palette();fill(a);}
void require(bool b,const char*message){if(!b)throw std::runtime_error(message);}
std::uint32_t bgr(std::uint32_t argb){return ((argb>>16)&255)|(argb&0xff00)|((argb&255)<<16);}
}
int main(){using namespace ps2::native;
 std::setvbuf(stdout,nullptr,_IONBF,0);std::setvbuf(stderr,nullptr,_IONBF,0);
 try {
  const auto dma=*DMA_REG_CTRL;*DMA_REG_CTRL=dma|1u;asm volatile("sync.l":::"memory");
  require(*DMA_REG_CTRL==(dma|1u),"DMA ENABLE FAILED");
  Display display;require(display.initialize(),display.error());display.set_vu0_packing(true);
  static std::array<Asset,101> assets;unsigned count=0;
  for(unsigned w:{8u,16u,32u,64u,128u,256u})for(unsigned h:{8u,16u,32u,64u,128u,256u}){
   initialize(assets[count],w,h,count);++count;}
  for(unsigned i=0;i<40;++i){initialize(assets[count],64,64,count);++count;}
  require(count==76,"ORIGINAL ASSET COUNT");assets[0].material.checker=true;
  for(unsigned i=76;i<101;++i)initialize(assets[i],256,256,i);
  static std::array<Triangle,152> triangles;std::array<std::uint32_t,64> pixels;
  unsigned samples=0;std::uint64_t hash=1469598103934665603ull;
  const auto draw=[&](const char*phase,unsigned first,unsigned n,unsigned coordinate,bool expect_warm){
   for(unsigned i=0;i<n;++i){const auto&a=assets[first+i];const float x=float(8+(i%8)*60),y=float(64+(i/8)*28);
    const unsigned tx=coordinate==0?a.texture.width-1:coordinate==1?a.texture.width/2:0;
    const unsigned ty=coordinate==0?a.texture.height-1:coordinate==1?a.texture.height/2:0;
    const float u=float(tx)+.25f,v=float(ty)+.25f;
    const Vertex p0{x,y,u,v,1,.5f},p1{x+44,y,u,v,1,.5f},p2{x+44,y+20,u,v,1,.5f},p3{x,y+20,u,v,1,.5f};
    triangles[2*i]={{p0,p1,p2},&a.material};triangles[2*i+1]={{p0,p2,p3},&a.material};
   }
   require(display.present({std::span(triangles).first(2*n),0xff101923u,true}),display.error());
   const auto stats=display.stats();
   if(expect_warm)require(!stats.texture_uploads&&!stats.texture_evictions,"RESIDENT FRAME REUPLOADED TEXTURES");
   unsigned mismatches=0;
   for(unsigned i=0;i<n;++i){const auto&a=assets[first+i];
    const unsigned x=(8+(i%8)*60+22)*640/frame_width-4;
    const unsigned y=(64+(i/8)*28+10)*display.output_height()/frame_height-4;
    require(display.readback_rect(x,y,8,8,pixels)==ReadbackStatus::ok,display.error());
    const unsigned tx=coordinate==0?a.texture.width-1:coordinate==1?a.texture.width/2:0;
    const unsigned ty=coordinate==0?a.texture.height-1:coordinate==1?a.texture.height/2:0;
    const auto color=bgr(a.material.palette[texel(tx,ty,a.seed)]);
    for(unsigned row=0;row<8;++row)for(unsigned col=0;col<8;++col){
     const bool enabled=!a.material.checker||((((x+col)*frame_width/640)^((y+row)*frame_height/display.output_height()))&1u);
     const auto expected=enabled?color:0x00231910u;const auto got=pixels[row*8+col]&0xffffffu;
     mismatches+=got!=expected;hash=(hash^got)*1099511628211ull;++samples;
    }
   }
   std::printf("TEXTURE CACHE PIXELS %s phase=%s images=%u uploads=%lu bytes=%llu hits=%lu evictions=%lu pages=%lu mismatch=%u\n",
     mismatches?"FAIL":"PASS",phase,n,(unsigned long)stats.texture_uploads,(unsigned long long)stats.texture_bytes,
     (unsigned long)stats.texture_hits,(unsigned long)stats.texture_evictions,(unsigned long)stats.texture_pages,mismatches);
   require(!mismatches,"TEXTURE PAGE PIXEL MISMATCH");return stats;
  };
  draw("cold_last_texels",0,76,0,false);
  draw("warm_middle_texels",0,76,1,true);
  draw("warm_first_texels",0,76,2,true);
  require(draw("forced_eviction",76,25,0,false).texture_evictions>0,"EVICTION PATH NOT EXERCISED");
  require(draw("restored_last_texels",0,76,0,false).texture_uploads>0,"RESTORE DID NOT REUPLOAD");
  draw("restored_warm",0,76,1,true);
  ++assets[0].seed;++assets[0].texture.generation;fill(assets[0]);
  require(draw("changed_generation",0,76,0,false).texture_uploads==1,"GENERATION CHANGE NOT ISOLATED");
  display.invalidate_textures();++assets[0].seed;fill(assets[0]);
  require(draw("invalidated_reused_storage",0,76,2,false).texture_uploads==77,"INVALIDATION DID NOT UPLOAD ALL ASSETS");
  std::printf("TEXTURE CACHE FIXTURE PASS frames=8 samples=%u hash=%016llx vram_bytes=%lu output_height=%u; original assets and sampled GS pixels only\n",
   samples,(unsigned long long)hash,(unsigned long)display_vram_bytes(display.output_height()),display.output_height());
  while(true)SleepThread();
 }catch(const std::exception&e){std::printf("TEXTURE CACHE FIXTURE FAIL: %s\n",e.what());while(true)SleepThread();}
}
