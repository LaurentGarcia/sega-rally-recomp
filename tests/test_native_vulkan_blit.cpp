// Compare actual X11 presentation through SDL Vulkan and the direct backend.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/gpu/gpu_renderer.h"
#include "app/gpu_device.h"
#include "app/native/vulkan/vulkan_presenter.h"
#include "model2_native_fixture.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
unsigned checks=0;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
void events(){SDL_Event event;while(SDL_PollEvent(&event)){}}
std::vector<uint32_t> capture(SDL_Window *window,int expected_width,int expected_height){
    const auto properties=SDL_GetWindowProperties(window);
    auto *display=static_cast<Display *>(SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_X11_DISPLAY_POINTER,nullptr));
    const auto native=Window(SDL_GetNumberProperty(properties,SDL_PROP_WINDOW_X11_WINDOW_NUMBER,0));
    require(display&&native,"independent readback requires an X11 window");
    int width=0,height=0;require(SDL_GetWindowSizeInPixels(window,&width,&height),"window pixel extent");
    require(width==expected_width&&height==expected_height,"exact requested physical dimensions");
    XSync(display,False);
    auto *image=XGetImage(display,native,0,0,unsigned(width),unsigned(height),AllPlanes,ZPixmap);
    require(image,"X11 presented image available");
    require(image->red_mask==0xff0000&&image->green_mask==0xff00&&image->blue_mask==0xff,"X11 RGB channel layout");
    std::vector<uint32_t> pixels(size_t(width)*height);std::set<unsigned long> colors;size_t nonblack=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const auto pixel=XGetPixel(image,x,y)&0xffffff;
        pixels[size_t(y)*width+x]=uint32_t(pixel)|0xff000000u;
        colors.insert(pixel);nonblack+=pixel!=0;
    }
    XDestroyImage(image);require(nonblack>10000&&colors.size()>3,"displayed scene is nonempty and colored");
    return pixels;
}
struct SdlRenderer {
    SDL_Window *window;SDL_GPUDevice *device=nullptr;SDL_GPUTexture *screen=nullptr;app::GpuRenderer renderer;
    explicit SdlRenderer(SDL_Window *value):window(value){
        device=app::create_gpu_device("vulkan");require(device,"SDL Vulkan device");
        require(std::string(SDL_GetGPUDeviceDriver(device))=="vulkan","baseline is Vulkan");
        require(SDL_ClaimWindowForGPUDevice(device,window),"SDL GPU window claim");
        require(SDL_SetGPUSwapchainParameters(device,window,SDL_GPU_SWAPCHAINCOMPOSITION_SDR,SDL_GPU_PRESENTMODE_VSYNC),"SDL SDR FIFO presentation");
        require(SDL_GetGPUSwapchainTextureFormat(device,window)==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,"baseline BGRA8 UNORM swapchain");
        require(renderer.init(device,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM),renderer.error().c_str());
        SDL_GPUTextureCreateInfo texture{};texture.type=SDL_GPU_TEXTURETYPE_2D;texture.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        texture.width=496;texture.height=384;texture.layer_count_or_depth=texture.num_levels=1;
        texture.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        screen=SDL_CreateGPUTexture(device,&texture);require(screen,"baseline offscreen game texture");
    }
    ~SdlRenderer(){if(device){SDL_WaitForGPUIdle(device);renderer.shutdown();if(screen)SDL_ReleaseGPUTexture(device,screen);
        SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);}}
    bool draw(const rt::Video &video,bool linear){
        auto *command=SDL_AcquireGPUCommandBuffer(device);require(command,"baseline command buffer");
        renderer.render(command,screen,496,384,video);
        SDL_GPUTexture *swapchain=nullptr;Uint32 width=0,height=0;
        require(SDL_WaitAndAcquireGPUSwapchainTexture(command,window,&swapchain,&width,&height),"baseline acquire");
        if(swapchain){
            const float scale=std::min(width/496.f,height/384.f);
            const Uint32 w=std::max(1u,Uint32(496*scale)),h=std::max(1u,Uint32(384*scale));
            SDL_GPUBlitInfo blit{};blit.source.texture=screen;blit.source.w=496;blit.source.h=384;
            blit.destination.texture=swapchain;blit.destination.x=(width-w)/2;blit.destination.y=(height-h)/2;blit.destination.w=w;blit.destination.h=h;
            blit.load_op=SDL_GPU_LOADOP_CLEAR;blit.clear_color={0,0,0,1};blit.filter=linear?SDL_GPU_FILTER_LINEAR:SDL_GPU_FILTER_NEAREST;
            SDL_BlitGPUTexture(command,&blit);
        }
        require(SDL_SubmitGPUCommandBuffer(command),"baseline submit");return swapchain!=nullptr;
    }
};
void run_case(const std::filesystem::path &out,bool native,int width,int height,bool linear){
    model2_fixture::Scene scene;scene.patterned_tiles(true);scene.cutout(true);
    scene.video.set_external_3d(true,true);scene.video.frame_start();
    const auto polygons=scene.polys(4);scene.video.screen_update(polygons,0,scene.mem);
    const auto name=std::to_string(width)+"x"+std::to_string(height)+(linear?"-linear":"-nearest");
    SDL_Window *window=SDL_CreateWindow(name.c_str(),width,height,native?SDL_WINDOW_VULKAN:0);
    require(window,SDL_GetError());
    try {
        std::unique_ptr<app::native::VulkanPresenter> presenter;
        std::unique_ptr<SdlRenderer> sdl;
        app::CrtSettings settings;settings.enabled=false;
        if(native){presenter=std::make_unique<app::native::VulkanPresenter>();require(presenter->init(window,true,true),presenter->error().c_str());}
        else sdl=std::make_unique<SdlRenderer>(window);
        unsigned presented=0;
        for(unsigned attempt=0;attempt<30&&presented<12;++attempt){
            events();
            if(presenter){const auto outcome=presenter->present(scene.video,1,false,settings,true,true,{},linear,false);
                require(outcome!=app::native::VulkanPresenter::Outcome::error&&outcome!=app::native::VulkanPresenter::Outcome::device_lost,presenter->error().c_str());
                presented+=outcome==app::native::VulkanPresenter::Outcome::presented;
            }else presented+=sdl->draw(scene.video,linear);
        }
        require(presented==12,"complete presentation warmup");
        if(presenter){presenter->drain();require(presenter->renderer().validation_errors()==0,"zero native Vulkan validation errors");}
        else require(SDL_WaitForGPUIdle(sdl->device),"baseline GPU idle");
        SDL_Delay(100);events();const auto first=capture(window,width,height);
        SDL_Delay(50);const auto second=capture(window,width,height);
        require(first==second,"stable independently displayed image");
        std::ofstream file(out/(name+".bgra"),std::ios::binary);
        file.write(reinterpret_cast<const char *>(first.data()),std::streamsize(first.size()*4));require(bool(file),"captured BGRA file saved");
        std::printf("case=%s backend=%s pixels=%zu presented=%u\n",name.c_str(),native?"native":"sdl",first.size(),presented);
    }catch(...){SDL_DestroyWindow(window);throw;}
    SDL_DestroyWindow(window);
}
}
int main(int argc,char **argv){try{
    require(argc==3,"usage: test_native_vulkan_blit <new-output-dir> --sdl|--native");
    const std::filesystem::path out=argv[1];const std::string backend=argv[2];
    require(backend=="--sdl"||backend=="--native","explicit backend required");
    require(!std::filesystem::exists(out),"output directory must be new");std::filesystem::create_directories(out);
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_GPU_DRIVER,"vulkan");require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    for(const auto dimensions:{std::array<int,2>{497,385},std::array<int,2>{317,245}})
        for(bool linear:{false,true})run_case(out,backend=="--native",dimensions[0],dimensions[1],linear);
    SDL_Quit();std::printf("PASS plain Vulkan presentation: 4 cases, %u checks\n",checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Vulkan blit fixture: %s\n",error.what());SDL_Quit();return 1;}}
