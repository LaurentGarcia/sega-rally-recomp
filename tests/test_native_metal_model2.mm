// Same original Model2 inputs consumed through separate native/SDL binaries.
#include "model2_native_fixture.h"
#include "model2_recorded_fixture.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef NATIVE_METAL_MODEL2
#include "app/native/metal/model2_metal.h"
#else
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/gpu/gpu_renderer.h"
#include "app/gpu_device.h"
#endif
namespace {
unsigned checks=0;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
#ifdef NATIVE_METAL_MODEL2
struct Backend {
    struct Pending { id<MTLCommandBuffer> command; id<MTLTexture> target; };
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = nil;
    app::native::MetalModel2 renderer;
    explicit Backend(bool) {
        require(device, "native Metal device"); queue = [device newCommandQueue]; require(queue, "Metal queue");
        require(renderer.init(device, MTLPixelFormatBGRA8Unorm), renderer.error().c_str());
        std::printf("Native Metal Model2 device: %s\n", device.name.UTF8String);
    }
    Pending encode(rt::Video &video, bool scanlines, int scale) { @autoreleasepool {
        const NSUInteger w = NSUInteger(video.width() * scale), h = NSUInteger(rt::Video::H * scale);
        auto *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
        desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageRenderTarget;
        auto target = [device newTextureWithDescriptor:desc]; require(target, "Metal color target");
        auto command = [queue commandBuffer]; require(command, "Metal command buffer");
        renderer.set_polygon_scanlines(scanlines);
        require(!renderer.encode(command,target,0,rt::Video::H,video,scale), "zero width rejects");
        require(!renderer.encode(command,target,video.width(),rt::Video::H,video,0), "zero scale rejects");
        auto *unretained = [MTLCommandBufferDescriptor new]; unretained.retainedReferences = NO;
        require(!renderer.encode([queue commandBufferWithDescriptor:unretained],target,video.width(),rt::Video::H,video,scale), "unretained command rejects");
        require(renderer.encode(command,target,video.width(),rt::Video::H,video,scale),renderer.error().c_str());
        require(command.status == MTLCommandBufferStatusNotEnqueued,"encode must not submit");
        return {command,target};
    }}
    std::vector<uint32_t> finish(const Pending &pending) {
        const auto command=pending.command;const auto target=pending.target;
        const NSUInteger w=target.width,h=target.height;
        [command commit]; [command waitUntilCompleted]; require(command.status == MTLCommandBufferStatusCompleted,"Metal completed");
        std::vector<uint32_t> pixels(w*h); [target getBytes:pixels.data() bytesPerRow:w*4 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];
        return pixels;
    }
    std::vector<uint32_t> draw(rt::Video &video,bool scanlines,int scale) { return finish(encode(video,scanlines,scale)); }
};
#else
struct Backend {
    SDL_GPUDevice *device=nullptr;app::GpuRenderer renderer;
    explicit Backend(bool){SDL_SetMainReady();SDL_SetHint(SDL_HINT_GPU_DRIVER,"metal");SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");require(SDL_Init(SDL_INIT_VIDEO),"SDL video init");device=app::create_gpu_device();require(device,"SDL Metal device");require(std::string(SDL_GetGPUDeviceDriver(device))=="metal","SDL baseline must use Metal");require(renderer.init(device,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM),renderer.error().c_str());}
    ~Backend(){if(device){SDL_WaitForGPUIdle(device);renderer.shutdown();SDL_DestroyGPUDevice(device);}SDL_Quit();}
    std::vector<uint32_t> draw(rt::Video &video,bool scanlines,int scale){
        const unsigned w=unsigned(video.width()*scale),h=unsigned(rt::Video::H*scale);
        SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;ti.width=w;ti.height=h;ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;ti.layer_count_or_depth=ti.num_levels=1;
        auto *target=SDL_CreateGPUTexture(device,&ti);require(target,"SDL target");renderer.set_polygon_scanlines(scanlines);
        auto *command=SDL_AcquireGPUCommandBuffer(device);require(command,"SDL command");renderer.render(command,target,video.width(),rt::Video::H,video,scale);require(SDL_SubmitGPUCommandBuffer(command),"SDL render submit");
        SDL_GPUTransferBufferCreateInfo bi{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*4,0};auto *buffer=SDL_CreateGPUTransferBuffer(device,&bi);require(buffer,"SDL readback");command=SDL_AcquireGPUCommandBuffer(device);require(command,"SDL copy command");auto *copy=SDL_BeginGPUCopyPass(command);require(copy,"SDL copy pass");SDL_GPUTextureRegion src{};src.texture=target;src.w=w;src.h=h;src.d=1;SDL_GPUTextureTransferInfo dst{};dst.transfer_buffer=buffer;dst.pixels_per_row=w;dst.rows_per_layer=h;SDL_DownloadFromGPUTexture(copy,&src,&dst);SDL_EndGPUCopyPass(copy);
        auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"SDL fence");require(SDL_WaitForGPUFences(device,true,&fence,1),"SDL complete");
        auto *bytes=static_cast<const uint32_t *>(SDL_MapGPUTransferBuffer(device,buffer,false));require(bytes,"SDL mapped");std::vector<uint32_t> pixels(bytes,bytes+size_t(w)*h);
        SDL_UnmapGPUTransferBuffer(device,buffer);SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,buffer);SDL_ReleaseGPUTexture(device,target);return pixels;
    }
};
#endif
void save(const std::filesystem::path &out,unsigned id,const std::vector<uint32_t> &pixels){
    char name[64];std::snprintf(name,sizeof name,"scene-%02u.bgra",id);std::ofstream f(out/name,std::ios::binary);f.write(reinterpret_cast<const char *>(pixels.data()),std::streamsize(pixels.size()*4));require(bool(f),"output bytes");
    require(std::count_if(pixels.begin(),pixels.end(),[](uint32_t p){return (p&0xffffff)!=0;})>100,"nonempty rendered image");
}
}
int main(int argc,char **argv){@autoreleasepool {try{
    require(argc==2||argc==3,"output directory [recorded-scene-directory] required");const auto out=std::filesystem::path(argv[1]);require(std::filesystem::is_directory(out),"output directory");Backend backend(false);model2_fixture::Scene scene;
    unsigned id=0;
    for(bool tiles:{false,true})for(bool scanlines:{false,true})for(unsigned variant:{0u,3u,4u})for(int scale:{1,2}){
        scene.patterned_tiles(tiles);scene.cutout(variant!=0);scene.video.set_external_3d(true,true);scene.video.frame_start();const auto polygons=scene.polys(variant);scene.video.screen_update(polygons,0,scene.mem);
        require(scene.video.gpu_pens()[1]!=scene.video.gpu_pens()[0],"synthetic tile palette has distinct colors");
        const auto pixels=backend.draw(scene.video,scanlines,scale);save(out,id,pixels);
        std::printf("scene=%u tiles=%u scanlines=%u variant=%u scale=%d pixels=%zu\n",id,unsigned(tiles),unsigned(scanlines),variant,scale,pixels.size());++id;
    }
    require(id==24,"complete matrix");scene.video.set_wide_margin(32);scene.video.set_hud_edges(true);
    auto hud=model2_fixture::polygon({{380,60},{460,60},{460,150},{380,150}},0,true);hud.z=100;scene.video.frame_start();const std::vector<rt::GeoPoly> hud_polygons{hud};scene.video.screen_update(hud_polygons,0,scene.mem);require(scene.video.cpu_front(),"CPU front path selected");save(out,id++,backend.draw(scene.video,true,1));
    scene.video.set_hud_edges(false);scene.video.set_wide_margin(0);scene.video.frame_start();const std::vector<rt::GeoPoly> empty;scene.video.screen_update(empty,0,scene.mem);save(out,id++,backend.draw(scene.video,false,1));
    require(id==26,"26 complete scenes");
#ifdef NATIVE_METAL_MODEL2
    // Both encodes finish before either command is submitted; mutate the same
    // Video in between and destroy its polygon views before GPU execution.
    // Each command must retain its own complete immutable snapshot.
    Backend::Pending pending_a,pending_b;
    {
        scene.patterned_tiles(false);scene.cutout(false);scene.video.frame_start();
        const auto polygons=scene.polys(0);scene.video.screen_update(polygons,0,scene.mem);
        pending_a=backend.encode(scene.video,false,1);
    }
    {
        scene.patterned_tiles(true);scene.cutout(true);scene.video.frame_start();
        const auto polygons=scene.polys(3);scene.video.screen_update(polygons,0,scene.mem);
        pending_b=backend.encode(scene.video,true,2);
    }
    require(backend.finish(pending_b)==model2_recorded_fixture::read<uint32_t>(out/"scene-21.bgra"),"second queued snapshot unchanged");
    require(backend.finish(pending_a)==model2_recorded_fixture::read<uint32_t>(out/"scene-00.bgra"),"first queued snapshot unchanged");
    std::puts("PASS two unsubmitted immutable snapshots, reverse submission order");
#endif
    if(argc==3) {
        for(const auto *directory : {"neutral/1634","race/2329","race/2599"}) {
            model2_recorded_fixture::Scene recorded(std::filesystem::path(argv[2])/directory);
            auto cpu=recorded.video(false); require(cpu->screen()==model2_recorded_fixture::read<uint32_t>(recorded.path/"cpu.bgra"),"historical CPU guard");
            auto video=recorded.video(true);recorded.check_composition(*video);
            for(bool spans:{false,true}) for(int scale:{1,2}) {
                const auto pixels=backend.draw(*video,spans,scale);save(out,id++,pixels);
                if(scale==1) {
                    const auto reference=model2_recorded_fixture::read<uint32_t>(recorded.path/(spans?"cpu.bgra":"current-off.bgra"));
                    require(pixels==reference,spans?"recorded scanline CPU parity":"recorded default SDL guard");
                }
                std::printf("recorded frame=%u scanlines=%u scale=%d pixels=%zu\n",recorded.frame,unsigned(spans),scale,pixels.size());
            }
        }
        require(id==38,"38 complete scenes");
    }
    std::printf("PASS %u Model2 Metal scenes, %u checks\n",id,checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Model2 Metal fixture: %s\n",error.what());return 1;}}}
