// Real SDL window, direct Metal game/CRT/Settings, and private-free test inputs.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/native/metal/metal_presenter.h"
#include "app/segarally_display.h"
#include "model2_native_fixture.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstdio>

namespace {
unsigned checks = 0;
void require(bool ok, const std::string &message) {
    ++checks; if (!ok) throw std::runtime_error(message);
}
std::vector<char> read(const std::filesystem::path &path) {
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"capture exists"); return {std::istreambuf_iterator<char>(file),{}};
}
void events(app::DisplayPanel &panel) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) panel.process_event(event);
}
}
int main(int argc,char **argv) { try {
    require(argc==2,"output directory required"); const std::filesystem::path output=argv[1];
    require(std::filesystem::is_directory(output),"output directory exists");
    SDL_SetMainReady(); require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    auto *window=SDL_CreateWindow("Sega Rally direct Metal presentation check",640,480,
        SDL_WINDOW_METAL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_HIDDEN);
    require(window,SDL_GetError());
    app::native::MetalPresenter presenter;app::DisplayPanel panel;app::DisplaySettings settings;
    require(!presenter.init(nullptr),"null window rejected");
    require(presenter.init(window),presenter.error());
    require(panel.init_metal(window,presenter.metal_device(),output,settings),panel.error());
    require(!presenter.capture(output/"before-frame.ppm"),"capture before frame rejected");
    model2_fixture::Scene scene;scene.patterned_tiles(true);scene.cutout(true);
    auto polygons=scene.polys(3);scene.video.set_external_3d(true,true);
    scene.video.frame_start();scene.video.screen_update(polygons,0,scene.mem);
    settings.accurate_textures=true;
    require(presenter.render(scene.video,settings,true,true,panel),presenter.error());
    require(presenter.capture(output/"hidden.ppm"),presenter.error());
    require(presenter.stats().skipped_drawables==1&&presenter.stats().presented_frames==0,"hidden offscreen frame");
    const auto original=read(output/"hidden.ppm");
    require(original.size()==15+496*384*3,"raw PPM dimensions");

    require(SDL_ShowWindow(window),SDL_GetError());
    require(SDL_RaiseWindow(window),SDL_GetError());
    for(unsigned frame=0;frame<18;++frame) {
        events(panel);
        if(frame==3) {settings.crt.enabled=true;panel.set_open(true);}
        if(frame==6) {settings.crt.enabled=false;settings.linear_filter=false;settings.integer_scale=true;}
        if(frame==9) {require(SDL_SetWindowSize(window,920,540),SDL_GetError());settings.linear_filter=true;}
        if(frame==12) {panel.set_open(false);settings.crt.enabled=true;settings.integer_scale=false;}
        require(presenter.render(scene.video,settings,false,true,panel),presenter.error());
        SDL_Delay(10);
    }
    require(presenter.wait_idle(),presenter.error());
    require(presenter.stats().presented_frames>=6,"real drawables presented");
    require(presenter.stats().game_frames==1,"paused settings reuse game frame");
    require(presenter.capture(output/"display-effects.ppm"),presenter.error());
    require(original==read(output/"display-effects.ppm"),"raw capture unaffected by CRT, UI, filters and resize");

    settings.accurate_textures=false;
    require(presenter.render(scene.video,settings,false,true,panel),presenter.error());
    require(presenter.stats().game_frames==2,"paused accurate-texture toggle redraws");
    settings.accurate_textures=true;
    require(presenter.render(scene.video,settings,false,true,panel),presenter.error());
    require(presenter.stats().game_frames==3,"second accurate-texture toggle redraws");
    require(presenter.capture(output/"accurate-restored.ppm"),presenter.error());
    require(original==read(output/"accurate-restored.ppm"),"accurate-mode restored capture");

    require(SDL_HideWindow(window),SDL_GetError());events(panel);
    scene.patterned_tiles(false);scene.cutout(false);polygons=scene.polys(0);
    scene.video.frame_start();scene.video.screen_update(polygons,0,scene.mem);
    require(presenter.render(scene.video,settings,true,true,panel),presenter.error());
    require(presenter.capture(output/"hidden-update.ppm"),presenter.error());
    require(original!=read(output/"hidden-update.ppm"),"hidden frame update changes capture");
    require(presenter.stats().skipped_drawables>=2,"hidden drawable skipped");
    require(presenter.wait_idle(),presenter.error());
    const auto counts=presenter.stats();panel.shutdown();presenter.shutdown();
    require(presenter.wait_idle(),"drain after shutdown safe");presenter.shutdown();
    SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS direct Metal presenter: %u checks, game=%llu presented=%llu skipped=%llu\n",checks,
        static_cast<unsigned long long>(counts.game_frames),static_cast<unsigned long long>(counts.presented_frames),
        static_cast<unsigned long long>(counts.skipped_drawables));
    return 0;
} catch(const std::exception &error) {std::fprintf(stderr,"Metal presenter: %s\n",error.what());return 1;} }
