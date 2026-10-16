// Original scene, native Wayland WSI. SDL_GPU and X11 are disabled for this fixture.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/native/vulkan/vulkan_presenter.h"
#include "model2_native_fixture.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
unsigned checks=0;
void require(bool condition,const char *message) {++checks;if(!condition)throw std::runtime_error(message);}
void pump() {SDL_Event event;while(SDL_PollEvent(&event)){};}
void phase(const char *message) {std::fprintf(stderr,"Wayland phase: %s\n",message);std::fflush(stderr);}
std::string bytes(const std::filesystem::path &path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}

int main(int argc,char **argv) {try {
    require(argc==2,"new output directory argument required");const auto out=std::filesystem::path(argv[1]);
    require(std::filesystem::is_directory(out),"runner creates output directory");
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    require(std::string(SDL_GetCurrentVideoDriver())=="wayland","native Wayland video backend selected");
    for(int i=0;i<SDL_GetNumVideoDrivers();++i)require(std::string(SDL_GetVideoDriver(i))!="x11","fixture must not contain X11 fallback");
    SDL_Window *window=SDL_CreateWindow("Native Wayland Vulkan fixture",640,480,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE);
    require(window,SDL_GetError());require(SDL_SyncWindow(window),SDL_GetError());pump();
    require(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,nullptr),"native wl_surface exists");
    {
        app::native::VulkanPresenter presenter;require(presenter.init(window,true,true),presenter.error().c_str());
        std::fprintf(stderr,"Vulkan CPU device: %s\n",presenter.renderer().device_name().c_str());
        model2_fixture::Scene scene;scene.patterned_tiles(true);scene.cutout(true);scene.video.set_external_3d(true,true);
        std::vector<rt::GeoPoly> polygons=scene.polys(4);scene.video.frame_start();scene.video.screen_update(polygons,0,scene.mem);
        app::CrtSettings crt;unsigned overlays=0;auto overlay=[&](VkCommandBuffer command) {
            VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;clear.colorAttachment=0;
            clear.clearValue.color.float32[0]=clear.clearValue.color.float32[3]=1;
            const VkClearRect rectangle{{{20,20},{12,12}},0,1};vkCmdClearAttachments(command,1,&clear,1,&rectangle);++overlays;
        };
        auto present=[&](bool update=true) {
            pump();
            const auto outcome=presenter.present(scene.video,1,false,crt,update,true,overlay);
            require(outcome==app::native::VulkanPresenter::Outcome::presented,"Wayland surface presentation completed");
        };
        app::native::VulkanModel2::Statistics warmed{};
        for(unsigned i=0;i<48;++i) {
            if(i==16)crt.enabled=true;
            present();
            if(i==8)warmed=presenter.renderer().statistics();
            if(i==30) {
                const auto now=presenter.renderer().statistics();
                require(now.pipeline_creations==warmed.pipeline_creations&&now.image_allocations==warmed.image_allocations&&now.buffer_allocations==warmed.buffer_allocations,"steady Wayland frames reuse resources");
                require(presenter.statistics().resize_idle_waits==0,"steady Wayland frames do not wait for whole device");
            }
        }
        require(presenter.capture(out/"before-pause.ppm"),"capture first cached image");
        for(auto &v:polygons.back().v)v.x+=35;
        scene.video.frame_start();scene.video.screen_update(polygons,0,scene.mem);
        for(unsigned i=0;i<4;++i)present(false);
        require(presenter.capture(out/"after-pause.ppm"),"capture paused cached image");
        require(bytes(out/"before-pause.ppm")==bytes(out/"after-pause.ppm"),"CPU scene mutations do not replace paused game image");
        const auto before=presenter.renderer().statistics().submissions;
        phase("hide begin");require(SDL_HideWindow(window),"hide native Wayland window");phase("hide returned");
        require(SDL_SyncWindow(window),"synchronize hide");phase("hide sync returned");pump();phase("hide events pumped");
        require(presenter.present(scene.video,1,false,crt,true,true)==app::native::VulkanPresenter::Outcome::paused,"hidden Wayland game still updates offscreen");
        phase("hidden present returned");
        require(presenter.renderer().statistics().submissions==before+1,"hidden game queued exactly one offscreen update");
        phase("show begin");require(SDL_ShowWindow(window),"restore native Wayland window");phase("show returned");
        require(SDL_SyncWindow(window),"synchronize restore");phase("show sync returned");
        for(unsigned i=0;i<4;++i){phase("restored present begin");present(false);phase("restored present returned");}
        for(const auto &[width,height]:{std::pair{901,601},std::pair{317,245},std::pair{640,480}}) {
            phase("resize begin");require(SDL_SetWindowSize(window,width,height),"request Wayland resize");phase("resize returned");
            require(SDL_SyncWindow(window),"synchronize configured Wayland extent");phase("resize sync returned");
            int actual_width=0,actual_height=0;require(SDL_GetWindowSize(window,&actual_width,&actual_height),"read configured size");
            require(actual_width==width&&actual_height==height,"compositor accepted requested window extent");
            for(unsigned i=0;i<4;++i)present(false);
        }
        presenter.drain();require(presenter.capture(out/"restored.ppm"),"capture updated game after restore");
        require(bytes(out/"before-pause.ppm")!=bytes(out/"restored.ppm"),"hidden game update survived restore");
        const auto r=presenter.renderer().statistics();const auto p=presenter.statistics();
        require(r.pipeline_creations==4&&p.pipeline_creations==1,"one set of pipelines across native Wayland resize");
        require(p.presentations==68&&overlays==68&&p.swapchain_creations>=4,"native surface, overlays and resize were exercised");
        require(presenter.renderer().validation_errors()==0,"no Vulkan synchronization/API validation errors");
        std::ofstream report(out/"metrics.json");report<<"{\"video_driver\":\"wayland\",\"checks\":"<<(checks+1)<<",\"presentations\":"<<p.presentations
            <<",\"swapchains\":"<<p.swapchain_creations<<",\"resize_idle_waits\":"<<p.resize_idle_waits
            <<",\"model2_pipelines\":"<<r.pipeline_creations<<",\"crt_pipelines\":"<<p.pipeline_creations
            <<",\"validation_errors\":"<<presenter.renderer().validation_errors()<<",\"scope\":\"Native Wayland surface and Vulkan submissions using original synthetic inputs. Captures are raw offscreen images, not compositor screenshots. No physical display, input, audio, game or vendor GPU validation.\"}\n";
        report.flush();require(bool(report),"write metrics");
        std::printf("PASS native Wayland: %u checks, %llu presentations\n",checks,static_cast<unsigned long long>(p.presentations));
    }
    SDL_DestroyWindow(window);SDL_Quit();return 0;
}catch(const std::exception &error) {std::fprintf(stderr,"Wayland fixture: %s\n",error.what());SDL_Quit();return 1;}}
