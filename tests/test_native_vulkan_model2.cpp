// Same original Model2 inputs consumed through separate native/SDL binaries.
#include "model2_native_fixture.h"
#include "model2_recorded_fixture.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef NATIVE_VULKAN_MODEL2
#include "app/native/vulkan/model2_vulkan.h"
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
#ifdef NATIVE_VULKAN_MODEL2
struct Backend {
    app::native::VulkanModel2 renderer;
    explicit Backend(bool software,bool validation){require(renderer.init(software,validation),"native init");std::printf("Native Vulkan Model2 device: %s\n",renderer.device_name().c_str());}
    std::vector<uint32_t> draw(rt::Video &video,bool scanlines,int scale){
        bool rejected=false;try{renderer.render(video,0,rt::Video::H,scale,scanlines);}catch(const std::runtime_error &){rejected=true;}
        require(rejected,"zero size rejects");
        for(const auto dimensions:{std::array<int,2>{video.width()+1,rt::Video::H},std::array<int,2>{video.width(),rt::Video::H+1}}){
            rejected=false;try{renderer.render(video,dimensions[0],dimensions[1],scale,scanlines);}catch(const std::runtime_error &){rejected=true;}
            require(rejected,"positive dimensions must match Video");
        }
        auto pixels=renderer.render(video,video.width(),rt::Video::H,scale,scanlines);
        require(renderer.validation_errors()==0,"zero Vulkan validation errors");return pixels;
    }
};
#else
struct Backend {
    SDL_GPUDevice *device=nullptr;app::GpuRenderer renderer;
    explicit Backend(bool,bool){SDL_SetMainReady();SDL_SetHint(SDL_HINT_GPU_DRIVER,"vulkan");require(SDL_Init(SDL_INIT_VIDEO),"SDL video init");device=app::create_gpu_device();require(device,"SDL Vulkan device");require(std::string(SDL_GetGPUDeviceDriver(device))=="vulkan","SDL baseline must use Vulkan");require(renderer.init(device,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM),renderer.error().c_str());}
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
int main(int argc,char **argv){try{
    require(argc>=2,"output directory required");
    bool software=false,validation=false;std::vector<std::filesystem::path> recordings;
    for(int i=2;i<argc;++i){const std::string option=argv[i];
        if(option=="--require-software")software=true;
        else if(option=="--require-validation")validation=true;
        else if(option=="--scene"&&i+1<argc)recordings.emplace_back(argv[++i]);
        else throw std::runtime_error("unknown/incomplete argument: "+option);
    }
    const auto out=std::filesystem::path(argv[1]);require(std::filesystem::is_directory(out),"output directory");
    Backend backend(software,validation);model2_fixture::Scene scene;
    unsigned id=0;
    for(bool tiles:{false,true})for(bool scanlines:{false,true})for(unsigned variant:{0u,3u,4u})for(int scale:{1,2}){
        scene.patterned_tiles(tiles);scene.cutout(variant!=0);scene.video.set_external_3d(true,true);scene.video.frame_start();const auto polygons=scene.polys(variant);scene.video.screen_update(polygons,0,scene.mem);
            require(std::count_if(scene.video.gpu_pens(),scene.video.gpu_pens()+rt::Video::kGpuPens,[](uint32_t p){return(p&0xffffff)!=0;})>100,"colored tile palette initialized");
        const auto pixels=backend.draw(scene.video,scanlines,scale);save(out,id,pixels);
        std::printf("scene=%u tiles=%u scanlines=%u variant=%u scale=%d pixels=%zu\n",id,unsigned(tiles),unsigned(scanlines),variant,scale,pixels.size());++id;
    }
    require(id==24,"complete matrix");scene.video.set_wide_margin(32);scene.video.set_hud_edges(true);
    auto hud=model2_fixture::polygon({{380,60},{460,60},{460,150},{380,150}},0,true);hud.z=100;scene.video.frame_start();const std::vector<rt::GeoPoly> hud_polygons{hud};scene.video.screen_update(hud_polygons,0,scene.mem);require(scene.video.cpu_front(),"CPU front path selected");save(out,id++,backend.draw(scene.video,true,1));
    scene.video.set_hud_edges(false);scene.video.set_wide_margin(0);scene.video.frame_start();const std::vector<rt::GeoPoly> empty_polygons;scene.video.screen_update(empty_polygons,0,scene.mem);save(out,id++,backend.draw(scene.video,false,1));
    require(id==26,"26 complete invented scenes");
    for(const auto &path:recordings){
        model2_recorded_fixture::Scene recorded(path);auto video=recorded.video(true);recorded.check_composition(*video);
        require(!recorded.polygons.empty(),"recorded geometry is nonempty");
        for(bool scanlines:{false,true})for(int scale:{1,2}){
            const auto pixels=backend.draw(*video,scanlines,scale);save(out,id,pixels);
            std::printf("recorded_scene=%u frame=%u scanlines=%u scale=%d polygons=%zu pixels=%zu\n",id,recorded.frame,unsigned(scanlines),scale,recorded.polygons.size(),pixels.size());++id;
        }
    }
    std::printf("PASS %u Model2 Vulkan scenes, %u checks\n",id,checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Model2 Vulkan fixture: %s\n",error.what());return 1;}}
