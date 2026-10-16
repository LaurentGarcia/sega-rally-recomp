// Original animated inputs; direct Vulkan presentation with SDL window only.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/native/vulkan/vulkan_presenter.h"
#include "model2_native_fixture.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <set>
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
namespace {
void require(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
std::vector<uint32_t> capture(SDL_Window *window,const std::filesystem::path &path){
#if defined(__linux__)
    const auto properties=SDL_GetWindowProperties(window);
    auto *display=static_cast<Display *>(SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_X11_DISPLAY_POINTER,nullptr));
    const auto native=Window(SDL_GetNumberProperty(properties,SDL_PROP_WINDOW_X11_WINDOW_NUMBER,0));
    require(display&&native,"X11 window for independent displayed-image readback");
    int width=0,height=0;require(SDL_GetWindowSizeInPixels(window,&width,&height),"capture size");
    XSync(display,False);auto *image=XGetImage(display,native,0,0,unsigned(width),unsigned(height),AllPlanes,ZPixmap);require(image,"displayed X11 image");
    std::vector<uint32_t> pixels(size_t(width)*height);std::set<unsigned long> colors;size_t nonblack=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto pixel=XGetPixel(image,x,y);pixels[size_t(y)*width+x]=uint32_t(pixel)|0xff000000u;colors.insert(pixel);nonblack+=(pixel&0xffffff)!=0;}
    XDestroyImage(image);require(nonblack>10000&&colors.size()>3,"nonempty colored image actually displayed");
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char *>(pixels.data()),std::streamsize(pixels.size()*4));require(bool(file),"display readback saved");
    std::printf("display_readback width=%d height=%d nonblack=%zu colors=%zu\n",width,height,nonblack,colors.size());return pixels;
#else
    (void)window;(void)path;return {};
#endif
}
}
int main(int argc,char **argv){try{
    require(argc==2,"output directory required");const auto out=std::filesystem::path(argv[1]);require(std::filesystem::is_directory(out),"output exists");
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    SDL_Window *window=SDL_CreateWindow("Direct Vulkan presentation fixture",640,480,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE);require(window,SDL_GetError());
    {
        app::native::VulkanPresenter presenter;require(presenter.init(window,true,true),presenter.error().c_str());
        model2_fixture::Scene scene;scene.patterned_tiles(true);scene.cutout(true);scene.video.set_external_3d(true,true);
        app::CrtSettings settings;app::native::VulkanModel2::Statistics warmed{};std::vector<uint32_t> plain,crt,paused_first,paused_second,nearest,linear;bool linear_filter=true,integer_scale=false;unsigned paused=0;std::vector<rt::GeoPoly> polygons;
        const auto context=presenter.ui_context();require(context.device&&context.render_pass&&context.image_count>=context.min_image_count&&context.min_image_count>=2,"ready UI context");
        auto overlay=[](VkCommandBuffer command){VkClearAttachment attachment{};attachment.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;attachment.colorAttachment=0;attachment.clearValue.color.float32[0]=attachment.clearValue.color.float32[3]=1;const VkClearRect rectangle{{{20,20},{12,12}},0,1};vkCmdClearAttachments(command,1,&attachment,1,&rectangle);};
        require(presenter.present(scene.video,1,false,settings,false,false,overlay)==app::native::VulkanPresenter::Outcome::presented,"empty game can present UI");
        for(unsigned i=0;i<100;++i){
            if(i==20)settings.enabled=true;
            if(i==34){settings.enabled=false;linear_filter=false;}
            if(i==36)linear_filter=true;
            if(i==37)integer_scale=true;
            if(i==38){integer_scale=false;settings.enabled=true;}
            if(i==40)require(SDL_SetWindowSize(window,900,600),"resize window");
            if(i==55){const auto before=presenter.renderer().statistics().submissions;require(SDL_HideWindow(window),"hide while game updates");require(presenter.present(scene.video,1,false,settings,true,true)==app::native::VulkanPresenter::Outcome::paused,"hidden game advances offscreen");require(presenter.renderer().statistics().submissions==before+1,"offscreen game frame submitted while hidden");require(SDL_ShowWindow(window),"restore updated frame");}
            if(i==60){require(SDL_HideWindow(window),"hide window");require(presenter.present(scene.video,1,false,settings,false,true)==app::native::VulkanPresenter::Outcome::paused,"hidden window pauses without submission");++paused;require(SDL_ShowWindow(window),"restore window");}
            if(i==75)require(SDL_SetWindowSize(window,1,1),"tiny window");
            if(i==78)require(SDL_SetWindowSize(window,640,480),"restore extent");
            SDL_Event event;while(SDL_PollEvent(&event)){}
            polygons=scene.polys(4);if(i>=40)for(auto &v:polygons.back().v)v.x+=float(i%13);
            scene.video.frame_start();scene.video.screen_update(polygons,0,scene.mem);
            const bool update_game=i<50||i>=58;
            const auto outcome=presenter.present(scene.video,1,false,settings,update_game,true,i>=90?std::function<void(VkCommandBuffer)>(overlay):std::function<void(VkCommandBuffer)>{},linear_filter,integer_scale);
            require(outcome!=app::native::VulkanPresenter::Outcome::error&&outcome!=app::native::VulkanPresenter::Outcome::device_lost,presenter.error().c_str());
            if(outcome==app::native::VulkanPresenter::Outcome::paused)++paused;
            if(i==12)warmed=presenter.renderer().statistics();
            if(i==32){const auto now=presenter.renderer().statistics();require(now.pipeline_creations==warmed.pipeline_creations&&now.buffer_allocations==warmed.buffer_allocations&&now.image_allocations==warmed.image_allocations,"steady frames reuse pipelines and allocations");require(presenter.statistics().resize_idle_waits==0,"no steady-frame device idle waits");}
            if(i==35||i==36||i==37){
                presenter.drain();SDL_Delay(30);auto pixels=capture(window,out/(i==35?"nearest.bgra":i==36?"linear.bgra":"integer.bgra"));
                if(i==35)nearest=std::move(pixels);else if(i==36)linear=std::move(pixels);
                else if(!pixels.empty())for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)if(x<72||x>=568||y<48||y>=432)require((pixels[y*640+x]&0xffffff)==0,"integer scale centers native-sized image");
            }
            if(i==33)require(presenter.capture(out/"raw-game.ppm"),"capture raw cached game without disrupting presentation");
            if(i==51||i==54){presenter.drain();SDL_Delay(30);auto pixels=capture(window,out/(i==51?"paused-first.bgra":"paused-second.bgra"));if(i==51)paused_first=std::move(pixels);else paused_second=std::move(pixels);}
            if(i==16||i==28){presenter.drain();SDL_Delay(30);auto pixels=capture(window,out/(i==16?"plain.bgra":"crt.bgra"));if(i==16)plain=std::move(pixels);else crt=std::move(pixels);}
        }
        presenter.drain();SDL_Delay(30);const auto restored=capture(window,out/"restored.bgra");
        require(paused_first==paused_second,"cached game pixels stay exact while CPU scene changes");
        require(restored.empty()||(restored[25*640+25]&0xffffff)==0xff0000,"caller overlay appears after CRT in active render pass");
        require(presenter.renderer().validation_errors()==0,"zero Vulkan synchronization/API validation errors");
        require(nearest.empty()||nearest!=linear,"nearest and linear filters produce distinct displayed pixels");
        require(plain.empty()||plain!=crt,"CRT settings change displayed pixels");
        const auto render=presenter.renderer().statistics();const auto present=presenter.statistics();
        require(render.pipeline_creations==4&&present.pipeline_creations==1,"pipelines created once including resize");
        require(present.presentations>=95&&present.swapchain_creations>=4&&paused>=1,"present resize pause restore coverage");
        std::ofstream metrics(out/"metrics.json");metrics<<"{\"presentations\":"<<present.presentations<<",\"swapchains\":"<<present.swapchain_creations<<",\"resize_idle_waits\":"<<present.resize_idle_waits<<",\"model2_pipeline_creations\":"<<render.pipeline_creations<<",\"crt_pipeline_creations\":"<<present.pipeline_creations<<",\"buffer_allocations\":"<<render.buffer_allocations<<",\"image_allocations\":"<<render.image_allocations<<",\"paused_checks\":"<<paused<<",\"validation_errors\":"<<presenter.renderer().validation_errors()<<"}\n";
        require(bool(metrics),"metrics saved");std::printf("PASS direct Vulkan presentation: %llu frames, %llu swapchains, 4+1 persistent pipelines, %u paused checks\n",static_cast<unsigned long long>(present.presentations),static_cast<unsigned long long>(present.swapchain_creations),paused);
    }
    SDL_DestroyWindow(window);SDL_Quit();return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Vulkan presentation: %s\n",error.what());SDL_Quit();return 1;}}
