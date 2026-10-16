// Actual Metal Settings pixels, mouse toggles, preferences and binding isolation.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app/segarally_display.h"
#include "app/segarally_display_preferences.h"
#include "app/segarally_controls.h"
#include "app/gpu_device.h"
#include "imgui.h"
#include "imgui_internal.h"
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks=0;
void require(bool ok,const char *message) {++checks;if(!ok)throw std::runtime_error(message);}
void *opaque(id object) {return (__bridge void *)object;}
struct Fixture {
    SDL_Window *window=nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue=nil;
    id<MTLTexture> target=nil;
    ImGuiContext *sentinel=nullptr,*panel_context=nullptr;
    app::DisplayPanel panel;
    app::DisplaySettings settings;
    app::segarally::Controls controls;
    int width=0,height=0;
    explicit Fixture(const std::filesystem::path &save) {
        require(device,"Metal device");queue=[device newCommandQueue];require(queue,"Metal queue");
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
        require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD),SDL_GetError());
        window=SDL_CreateWindow("Settings validation",992,768,SDL_WINDOW_METAL|SDL_WINDOW_HIDDEN);
        require(window,SDL_GetError());require(SDL_GetWindowSizeInPixels(window,&width,&height),"window pixel dimensions");
        auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        desc.storageMode=MTLStorageModeShared;desc.usage=MTLTextureUsageRenderTarget;
        target=[device newTextureWithDescriptor:desc];require(target,"Metal target");
        sentinel=ImGui::CreateContext();settings.crt.enabled=true;
        require(panel.init_metal(window,opaque(device),save,settings),panel.error().c_str());
        require(controls.init(save),controls.error().c_str());panel.attach_controls(&controls);
        require(panel.new_frame_metal(settings,nullptr),"closed panel needs no render pass");
        require(ImGui::GetCurrentContext()==sentinel,"registration preserves foreign context");
        draw([&] {panel_context=ImGui::GetCurrentContext();ImGui::Begin("Overlay fixture");ImGui::TextUnformatted("Overlay visible");ImGui::End();});
        require(panel_context && panel_context!=sentinel,"overlay has private panel context");
    }
    ~Fixture() {
        panel.shutdown();controls.shutdown();
        if(sentinel)ImGui::DestroyContext(sentinel);
        if(window)SDL_DestroyWindow(window);SDL_Quit();
    }
    std::vector<uint32_t> draw(const std::function<void()> &overlay={}) { @autoreleasepool {
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
        require(panel.new_frame_metal(settings,opaque(pass),overlay),panel.error().c_str());
        require(ImGui::GetCurrentContext()==sentinel,"new frame restores foreign context");
        auto command=[queue commandBuffer];auto encoder=[command renderCommandEncoderWithDescriptor:pass];require(encoder,"UI encoder");
        require(panel.render_metal(opaque(command),opaque(encoder)),panel.error().c_str());
        require(ImGui::GetCurrentContext()==sentinel,"render restores foreign context");
        [encoder endEncoding];[command commit];[command waitUntilCompleted];
        require(command.status==MTLCommandBufferStatusCompleted,"UI command completed");
        std::vector<uint32_t> pixels(size_t(width)*height);[target getBytes:pixels.data() bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
        return pixels;
    }}
    ImVec2 original_button() {
        ImGui::SetCurrentContext(panel_context);ImVec2 result{};bool found=false;
        for(auto *window:panel_context->Windows)if(std::strstr(window->Name,"/Display settings_")) {
            result=window->DC.CursorStartPos;result.x+=20;result.y+=10;found=true;break;
        }
        ImGui::SetCurrentContext(sentinel);require(found,"Display settings child exists");return result;
    }
    void mouse(ImVec2 point,bool down) {
        SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;move.motion.windowID=SDL_GetWindowID(window);move.motion.x=point.x;move.motion.y=point.y;
        panel.process_event(move);
        SDL_Event button{};button.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
        button.button.windowID=SDL_GetWindowID(window);button.button.button=SDL_BUTTON_LEFT;button.button.down=down;button.button.x=point.x;button.button.y=point.y;
        panel.process_event(button);draw();
    }
    void sdl_roundtrip(const std::filesystem::path &save) {
        auto *gpu=app::create_gpu_device("metal");require(gpu,"SDL Metal comparison device");
        require(panel.init(window,gpu,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,save,settings),"native to SDL panel transition");
        panel.set_open(true);require(panel.new_frame(settings),panel.error().c_str());
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        auto *texture=SDL_CreateGPUTexture(gpu,&info);require(texture,"SDL comparison target");
        auto *command=SDL_AcquireGPUCommandBuffer(gpu);require(command,"SDL UI command");panel.prepare(command);
        SDL_GPUColorTargetInfo color{};color.texture=texture;color.load_op=SDL_GPU_LOADOP_CLEAR;color.store_op=SDL_GPU_STOREOP_STORE;color.clear_color={0,0,0,1};
        auto *pass=SDL_BeginGPURenderPass(command,&color,1,nullptr);require(pass,"SDL UI pass");panel.render(command,pass);SDL_EndGPURenderPass(pass);
        SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,Uint32(width*height*4),0};
        auto *buffer=SDL_CreateGPUTransferBuffer(gpu,&transfer);require(buffer,"SDL UI readback");auto *copy=SDL_BeginGPUCopyPass(command);
        SDL_GPUTextureRegion source{};source.texture=texture;source.w=width;source.h=height;source.d=1;
        SDL_GPUTextureTransferInfo destination{};destination.transfer_buffer=buffer;destination.pixels_per_row=width;destination.rows_per_layer=height;
        SDL_DownloadFromGPUTexture(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
        auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence && SDL_WaitForGPUFences(gpu,true,&fence,1),"SDL UI complete");
        auto *pixels=static_cast<const uint32_t *>(SDL_MapGPUTransferBuffer(gpu,buffer,false));require(pixels,"SDL UI mapped");
        require(std::count_if(pixels,pixels+size_t(width)*height,[](uint32_t p){return(p&0xffffff)!=0;})>10000,"same-binary SDL Settings visible");
        SDL_UnmapGPUTransferBuffer(gpu,buffer);SDL_ReleaseGPUFence(gpu,fence);SDL_ReleaseGPUTransferBuffer(gpu,buffer);SDL_ReleaseGPUTexture(gpu,texture);
        require(panel.init_metal(window,opaque(device),save,settings),"SDL to native panel transition");
        SDL_DestroyGPUDevice(gpu);require(ImGui::GetCurrentContext()==sentinel,"backend transitions preserve foreign context");
        panel.set_open(true);const auto native=draw();require(std::count_if(native.begin(),native.end(),[](uint32_t p){return(p&0xffffff)!=0;})>10000,"native Settings renders after SDL shutdown");
    }
};
}
int main(int argc,char **argv) {@autoreleasepool {try {
    require(argc==2,"new output directory required");const auto out=std::filesystem::path(argv[1]);
    require(!std::filesystem::exists(out),"output must not exist");std::filesystem::create_directories(out);
    std::ofstream(out/"eeprom.bin")<<"EEPROM sentinel";std::ofstream(out/"backup.bin")<<"backup sentinel";
    {
        Fixture f(out);f.panel.set_open(true);const auto pixels=f.draw();
        require(std::count_if(pixels.begin(),pixels.end(),[](uint32_t p){return(p&0xffffff)!=0;})>10000,"Settings produces visible pixels");
        std::ofstream image(out/"settings.bgra",std::ios::binary);image.write(reinterpret_cast<const char *>(pixels.data()),std::streamsize(pixels.size()*4));
        const auto button=f.original_button();f.mouse(button,true);f.mouse(button,false);
        require(!f.settings.crt.enabled,"Original button disables CRT");
        ImGui::SetCurrentContext(f.panel_context);
        const float next=ImGui::CalcTextSize("Original").x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCurrentContext(f.sentinel);auto arcade=button;arcade.x+=next;
        f.mouse(arcade,true);f.mouse(arcade,false);require(f.settings.crt.enabled,"Arcade CRT button enables CRT");
        app::DisplaySettings restored;std::string error;
        require(app::display_preferences::load(out/"display.settings",restored,error) && app::display_preferences::equal(restored,f.settings),"UI changes persist exactly");
        f.controls.begin_binding(app::segarally::Action::Gas,app::segarally::Source::Keyboard);
        SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.windowID=SDL_GetWindowID(f.window);key.key.scancode=SDL_SCANCODE_B;key.key.key=SDLK_B;key.key.down=true;
        const bool consumed=f.controls.process_event(key);require(consumed,"Controls consumes binding key");
        require(f.panel.process_event(key,consumed),"binding key isolated from Settings");f.draw();
        require(f.controls.mapper.settings.bindings[size_t(app::segarally::Action::Gas)].key==SDL_SCANCODE_B,"new key binding retained");
        ImGui::SetCurrentContext(f.panel_context);require(!ImGui::IsKeyDown(ImGuiKey_B),"binding key not forwarded to ImGui");ImGui::SetCurrentContext(f.sentinel);
        key.key.scancode=SDL_SCANCODE_F1;require(!f.panel.process_event(key),"F1 remains host-owned");
        f.controls.begin_binding(app::segarally::Action::Brake,app::segarally::Source::Keyboard);f.panel.set_open(false);
        require(!f.controls.binding(),"closing Settings cancels binding");require(f.panel.new_frame_metal(f.settings,nullptr),"closed panel remains lazy");
        f.sdl_roundtrip(out);
        f.panel.shutdown();require(ImGui::GetCurrentContext()==f.sentinel,"shutdown restores foreign context");
        app::DisplaySettings loaded;require(f.panel.init_metal(f.window,opaque(f.device),out,loaded),"reinitialize native panel");
        require(app::display_preferences::equal(loaded,f.settings),"reopened native panel restores preferences");
    }
    for(const auto &[name,expected]:std::array<std::pair<const char *,const char *>,2>{{{"eeprom.bin","EEPROM sentinel"},{"backup.bin","backup sentinel"}}}) {
        std::ifstream file(out/name);std::string actual;std::getline(file,actual);require(actual==expected,"game saves unchanged");
    }
    std::printf("PASS Metal Settings: %u checks, GPU pixels, mouse CRT toggles, preferences and binding isolation\n",checks);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Metal Settings: %s\n",error.what());return 1;}}}
