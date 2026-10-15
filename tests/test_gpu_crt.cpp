// Synthetic presentation checks; optional saved-image preview uses no game code.
#include "app/gpu/crt_renderer.h"
#include "app/gpu_device.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool yes, const char *why) { if (!yes) throw std::runtime_error(std::string(why) + ": " + SDL_GetError()); }
struct Fixture {
    SDL_GPUDevice *device = nullptr;
    SDL_GPUTexture *source = nullptr, *target = nullptr;
    int sw = 0, sh = 0, tw = 0, th = 0;
    unsigned draws = 0;
    app::CrtRenderer crt;
    Fixture() {
        SDL_SetMainReady(); SDL_SetHint(SDL_HINT_GPU_DRIVER, "metal"); SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
        require(SDL_Init(SDL_INIT_VIDEO), "SDL"); device = app::create_gpu_device(); require(device, "device");
        require(crt.init(device, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM), crt.error().c_str());
    }
    ~Fixture() {
        if (device) { SDL_WaitForGPUIdle(device); crt.shutdown();
            if (source) SDL_ReleaseGPUTexture(device, source); if (target) SDL_ReleaseGPUTexture(device, target);
            SDL_DestroyGPUDevice(device); }
        SDL_Quit();
    }
    void setup(int w, int h, int ow, int oh, const std::vector<uint32_t> &pixels) {
        SDL_WaitForGPUIdle(device);
        if (source) SDL_ReleaseGPUTexture(device, source); if (target) SDL_ReleaseGPUTexture(device, target);
        sw=w;sh=h;tw=ow;th=oh;
        SDL_GPUTextureCreateInfo ti{}; ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        ti.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;ti.width=Uint32(w);ti.height=Uint32(h);ti.layer_count_or_depth=ti.num_levels=1;
        source=SDL_CreateGPUTexture(device,&ti);require(source,"source");ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;ti.width=Uint32(ow);ti.height=Uint32(oh);
        target=SDL_CreateGPUTexture(device,&ti);require(target,"target");
        SDL_GPUTransferBufferCreateInfo bi{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,Uint32(pixels.size()*4),0};
        auto *buffer=SDL_CreateGPUTransferBuffer(device,&bi);require(buffer,"upload buffer");
        void *p=SDL_MapGPUTransferBuffer(device,buffer,false);require(p,"upload map");std::memcpy(p,pixels.data(),pixels.size()*4);SDL_UnmapGPUTransferBuffer(device,buffer);
        auto *cmd=SDL_AcquireGPUCommandBuffer(device);require(cmd,"upload command");auto *pass=SDL_BeginGPUCopyPass(cmd);require(pass,"upload pass");
        SDL_GPUTextureTransferInfo from{};from.transfer_buffer=buffer;from.pixels_per_row=Uint32(w);from.rows_per_layer=Uint32(h);
        SDL_GPUTextureRegion to{};to.texture=source;to.w=Uint32(w);to.h=Uint32(h);to.d=1;
        SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);finish(cmd);SDL_ReleaseGPUTransferBuffer(device,buffer);
    }
    void finish(SDL_GPUCommandBuffer *cmd) {
        auto *f=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(f,"submit");require(SDL_WaitForGPUFences(device,true,&f,1),"wait");SDL_ReleaseGPUFence(device,f);
    }
    std::vector<uint32_t> read() {
        SDL_GPUTransferBufferCreateInfo bi{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,Uint32(tw*th*4),0};
        auto *b=SDL_CreateGPUTransferBuffer(device,&bi);require(b,"download buffer");auto *cmd=SDL_AcquireGPUCommandBuffer(device);require(cmd,"download command");
        auto *pass=SDL_BeginGPUCopyPass(cmd);require(pass,"download pass");SDL_GPUTextureRegion from{};from.texture=target;from.w=Uint32(tw);from.h=Uint32(th);from.d=1;
        SDL_GPUTextureTransferInfo to{};to.transfer_buffer=b;to.pixels_per_row=Uint32(tw);to.rows_per_layer=Uint32(th);SDL_DownloadFromGPUTexture(pass,&from,&to);SDL_EndGPUCopyPass(pass);finish(cmd);
        auto *p=static_cast<const uint32_t *>(SDL_MapGPUTransferBuffer(device,b,false));require(p,"download map");std::vector<uint32_t>out(p,p+tw*th);
        SDL_UnmapGPUTransferBuffer(device,b);SDL_ReleaseGPUTransferBuffer(device,b);return out;
    }
    std::vector<uint32_t> blit(const SDL_Rect &viewport, bool disabled_call=false) {
        require(++draws <= 20, "draw cap");
        auto *cmd=SDL_AcquireGPUCommandBuffer(device);require(cmd,"blit command");SDL_GPUBlitInfo b{};
        b.source.texture=source;b.source.w=Uint32(sw);b.source.h=Uint32(sh);b.destination.texture=target;
        b.destination.x=Uint32(viewport.x);b.destination.y=Uint32(viewport.y);b.destination.w=Uint32(viewport.w);b.destination.h=Uint32(viewport.h);
        b.load_op=SDL_GPU_LOADOP_CLEAR;b.clear_color={0,0,0,1};b.filter=SDL_GPU_FILTER_LINEAR;SDL_BlitGPUTexture(cmd,&b);
        if(disabled_call) {app::CrtRenderer uninitialized;app::CrtSettings off;require(uninitialized.render(cmd,nullptr,nullptr,viewport,off),"disabled bypass");}
        finish(cmd);return read();
    }
    std::vector<uint32_t> draw(const SDL_Rect &viewport, const app::CrtSettings &s, SDL_Point native={0,0}) {
        require(++draws <= 20, "draw cap");
        if(!native.x)native={sw,sh};auto *cmd=SDL_AcquireGPUCommandBuffer(device);require(cmd,"CRT command");
        require(crt.render(cmd,source,target,viewport,s,{sw,sh},native),crt.error().c_str());finish(cmd);return read();
    }
    void bars(const std::vector<uint32_t> &p,const SDL_Rect &v) {
        for(int y=0;y<th;++y)for(int x=0;x<tw;++x)if(x<v.x||x>=v.x+v.w||y<v.y||y>=v.y+v.h)require(p[size_t(y*tw+x)]==0xff000000u,"letterbox clear");
    }
    void ppm(const std::filesystem::path &path,const std::vector<uint32_t>&p) {
        std::ofstream f(path,std::ios::binary);f<<"P6\n"<<tw<<' '<<th<<"\n255\n";
        for(uint32_t c:p){char rgb[]{char(c>>16),char(c>>8),char(c)};f.write(rgb,3);}require(bool(f),"preview output");
    }
};
app::CrtSettings zero() { app::CrtSettings s;s.enabled=true;s.scanlines=s.mask=s.glow=s.curvature=s.chromatic=0;return s; }
unsigned max_error(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b) {
    require(a.size()==b.size(),"image size");unsigned m=0;for(size_t i=0;i<a.size();++i)for(unsigned s:{0u,8u,16u})m=std::max(m,unsigned(std::abs(int(a[i]>>s&255)-int(b[i]>>s&255))));return m;
}
} // namespace
int main(int argc,char **argv) { try {
    Fixture f;std::vector<uint32_t>pattern(32*24);
    for(unsigned y=0;y<24;++y)for(unsigned x=0;x<32;++x)pattern[y*32+x]=0xff000000u|((x*8)<<16)|((y*10)<<8)|(((x/4+y/3)&1)?224:16);
    const SDL_Rect v{5,7,96,72};f.setup(32,24,106,86,pattern);auto original=f.blit(v);require(f.blit(v,true)==original,"disabled must preserve exact blit");
    auto z=zero();auto flat=f.draw(v,z);require(max_error(flat,original)<=1,"zero-effects linear presentation");f.bars(flat,v);
    for(unsigned effect=0;effect<5;++effect){auto s=z;switch(effect){case 0:s.scanlines=.6f;break;case 1:s.mask=.5f;break;case 2:s.glow=.3f;break;case 3:s.curvature=.1f;break;default:s.chromatic=1;break;}
        auto p=f.draw(v,s);require(p!=flat,"each control changes output");f.bars(p,v);
        if(effect==3)require(p[size_t(v.y*f.tw+v.x)]==0xff000000u,"curved corner clear");}
    auto limited=z;limited.scanlines=limited.mask=2;limited.glow=limited.curvature=limited.chromatic=2;auto maximum=z;maximum.scanlines=maximum.mask=maximum.chromatic=1;maximum.glow=.5f;maximum.curvature=.15f;
    require(f.draw(v,limited)==f.draw(v,maximum),"finite strength clamping");
    auto invalid=z;invalid.scanlines=invalid.mask=invalid.glow=invalid.curvature=invalid.chromatic=std::numeric_limits<float>::quiet_NaN();require(f.draw(v,invalid)==flat,"nonfinite strength clamping");
    // Native/downscaled output does not alias the beam pattern. At 2x, beams resolve.
    std::vector<uint32_t>white(32*24,0xffffffffu);f.setup(32,24,64,48,white);auto beam=z;beam.scanlines=1;
    require(f.draw({0,0,32,24},beam)==f.draw({0,0,32,24},z),"native scanline attenuation");
    require(f.draw({0,0,16,12},beam)==f.draw({0,0,16,12},z),"downscale scanline attenuation");
    auto lines=f.draw({0,0,64,48},beam);require(lines[0]!=lines[64],"2x beam rows");
    auto mask=z;mask.mask=1;auto stripes=f.draw({0,0,64,48},mask);require(stripes[0]==0xffff0000u&&stripes[1]==0xff00ff00u&&stripes[2]==0xff0000ffu,"physical RGB mask phase");
    auto wider=std::vector<uint32_t>(64*48,0xffffffffu);f.setup(64,48,64,48,wider);require(f.draw({0,0,64,48},beam,{32,24})==lines,"native beam count independent of enhanced source");
    if(argc==3){std::ifstream in(argv[1],std::ios::binary);std::vector<uint32_t>image(496*384);in.read(reinterpret_cast<char*>(image.data()),std::streamsize(image.size()*4));require(bool(in)&&in.peek()==EOF,"saved scene image");
        const auto out=std::filesystem::path(argv[2]);require(std::filesystem::is_directory(out),"preview directory");f.setup(496,384,1152,896,image);SDL_Rect view{80,64,992,768};
        f.ppm(out/"original.ppm",f.blit(view));app::CrtSettings defaults;defaults.enabled=true;auto crt=f.draw(view,defaults);f.bars(crt,view);f.ppm(out/"crt-default.ppm",crt);}
    require(f.draws==(argc==3?20u:18u),"final draw count");
    std::printf("CRT synthetic checks PASS: draws=%u bypass, zero effects, five controls, bars, clamps, native/enhanced beam count, physical mask; no game execution\n",f.draws);return 0;
} catch(const std::exception&e){std::fprintf(stderr,"CRT test FAIL: %s\n",e.what());return 1;} }
