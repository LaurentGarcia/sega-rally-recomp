// ROM-free native Metal CRT comparison. Expected images come from the unchanged
// SDL fixture's readback, not a CPU reimplementation of the shader.
#include "app/native/metal/metal_crt.h"
#import <Foundation/Foundation.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
using app::native::PixelViewport;
namespace {
int checks=0;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
app::CrtSettings zero(){app::CrtSettings s;s.enabled=true;s.scanlines=s.mask=s.glow=s.curvature=s.chromatic=0;return s;}
struct Case{unsigned sw=32,sh=24,tw=106,th=86;PixelViewport viewport{5,7,96,72};app::CrtSettings settings=zero();app::native::PixelSize native{32,24};bool white=false;};
std::vector<Case> cases(){
    std::vector<Case> result{Case{}};
    for(unsigned effect=0;effect<5;++effect){Case c;switch(effect){case 0:c.settings.scanlines=.6f;break;case 1:c.settings.mask=.5f;break;case 2:c.settings.glow=.3f;break;case 3:c.settings.curvature=.1f;break;default:c.settings.chromatic=1;break;}result.push_back(c);}
    Case limited;limited.settings.scanlines=limited.settings.mask=limited.settings.glow=limited.settings.curvature=limited.settings.chromatic=2;result.push_back(limited);
    Case maximum;maximum.settings.scanlines=maximum.settings.mask=maximum.settings.chromatic=1;maximum.settings.glow=.5f;maximum.settings.curvature=.15f;result.push_back(maximum);
    Case invalid;invalid.settings.scanlines=invalid.settings.mask=invalid.settings.glow=invalid.settings.curvature=invalid.settings.chromatic=std::numeric_limits<float>::quiet_NaN();result.push_back(invalid);
    Case white;white.tw=64;white.th=48;white.white=true;white.viewport={0,0,32,24};white.settings.scanlines=1;result.push_back(white);
    white.settings=zero();result.push_back(white);white.viewport={0,0,16,12};white.settings.scanlines=1;result.push_back(white);
    white.settings=zero();result.push_back(white);white.viewport={0,0,64,48};white.settings.scanlines=1;result.push_back(white);
    white.settings=zero();white.settings.mask=1;result.push_back(white);white.settings=zero();white.settings.scanlines=1;white.sw=64;white.sh=48;result.push_back(white);
    return result;
}
std::vector<uint32_t> expected(const std::filesystem::path &directory,unsigned id,unsigned width,unsigned height){
    char name[64];std::snprintf(name,sizeof(name),"fixture-%02u.bgra",id);std::ifstream file(directory/name,std::ios::binary);
    std::array<uint32_t,2> size{};file.read(reinterpret_cast<char *>(size.data()),sizeof(size));require(size[0]==width&&size[1]==height,"expected dimensions");
    std::vector<uint32_t> pixels(size_t(width)*height);file.read(reinterpret_cast<char *>(pixels.data()),std::streamsize(pixels.size()*4));require(bool(file)&&file.peek()==EOF,"expected bytes");return pixels;
}
}
int main(int argc,char **argv){@autoreleasepool{try{
    require(argc==3,"expected-directory output-directory arguments");const auto out=std::filesystem::path(argv[2]);require(std::filesystem::is_directory(out),"output directory");
    auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"Metal device");auto queue=[device newCommandQueue];require(queue!=nil,"Metal queue");
    app::native::MetalCrt crt;app::CrtSettings off;
    require(crt.encode(nil,nil,nil,{},off,{}),"disabled must bypass all native resources");
    require(!crt.encode(nil,nil,nil,{},zero(),{}),"enabled missing resources reject");
    require(!crt.init(nil,MTLPixelFormatBGRA8Unorm),"missing device rejects");require(crt.init(device,MTLPixelFormatBGRA8Unorm),crt.error().c_str());
    std::ofstream metrics(out/"timing.json");metrics<<"{\"scope\":\"16 tiny offscreen CRT cases, correctness run; no comparative performance claim\",\"samples\":[";
    unsigned id=3;for(const auto &c:cases()){@autoreleasepool{
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:c.sw height:c.sh mipmapped:NO];
        descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageShaderRead;auto source=[device newTextureWithDescriptor:descriptor];require(source!=nil,"source texture");
        descriptor.width=c.tw;descriptor.height=c.th;descriptor.usage=MTLTextureUsageRenderTarget;auto target=[device newTextureWithDescriptor:descriptor];require(target!=nil,"target texture");
        std::vector<uint32_t> pixels(size_t(c.sw)*c.sh,0xffffffffu);
        if(!c.white)for(unsigned y=0;y<c.sh;++y)for(unsigned x=0;x<c.sw;++x)pixels[y*c.sw+x]=0xff000000u|((x*8)<<16)|((y*10)<<8)|(((x/4+y/3)&1)?224:16);
        [source replaceRegion:MTLRegionMake2D(0,0,c.sw,c.sh) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:c.sw*4];
        auto command=[queue commandBuffer];require(command!=nil,"command buffer");
        require(!crt.encode(command,source,source,c.viewport,c.settings,c.native),"source target alias rejects");
        auto outside=c.viewport;outside.w=std::numeric_limits<uint32_t>::max();require(!crt.encode(command,source,target,outside,c.settings,c.native),"overflowing viewport rejects");
        require(!crt.encode(command,source,target,c.viewport,c.settings,{}),"zero native size rejects");
        const auto start=std::chrono::steady_clock::now();require(crt.encode(command,source,target,c.viewport,c.settings,c.native),crt.error().c_str());
        const auto cpu=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        [command commit];[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,"native GPU completion");
        std::vector<uint32_t> result(size_t(c.tw)*c.th);[target getBytes:result.data() bytesPerRow:c.tw*4 fromRegion:MTLRegionMake2D(0,0,c.tw,c.th) mipmapLevel:0];
        char name[64];std::snprintf(name,sizeof(name),"fixture-%02u.bgra",id);std::ofstream image(out/name,std::ios::binary);const uint32_t dimensions[2]={c.tw,c.th};image.write(reinterpret_cast<const char *>(dimensions),sizeof(dimensions));image.write(reinterpret_cast<const char *>(result.data()),std::streamsize(result.size()*4));require(bool(image),"actual image write");
        const auto reference=expected(argv[1],id,c.tw,c.th);size_t differences=0;for(size_t i=0;i<result.size();++i)differences+=result[i]!=reference[i];
        std::printf("case=%u pixels=%zu differences=%zu encode_cpu_ns=%lld gpu_ns=%.0f\n",id,result.size(),differences,static_cast<long long>(cpu),(command.GPUEndTime-command.GPUStartTime)*1e9);
        require(result==reference,"native Metal CRT must equal SDL Metal pixels exactly");
        if(id>3)metrics<<',';metrics<<"{\"case\":"<<id<<",\"encode_cpu_ns\":"<<cpu<<",\"gpu_ns\":";
        if(command.GPUStartTime>0&&command.GPUEndTime>=command.GPUStartTime)metrics<<(command.GPUEndTime-command.GPUStartTime)*1e9;else metrics<<"null";
        metrics<<'}';++id;
    }}
    require(id==19,"all 16 CRT images compared");metrics<<"]}\n";require(bool(metrics),"timing output");
    crt.shutdown();require(crt.encode(nil,nil,nil,{},off,{}),"disabled after shutdown bypasses");require(!crt.encode(nil,nil,nil,{},zero(),{}),"enabled after shutdown rejects");
    std::printf("PASS native Metal CRT: 16 exact SDL pixel matches, %d checks; no SDL linked, no game execution\n",checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"native Metal CRT FAIL: %s\n",error.what());return 1;}}}
