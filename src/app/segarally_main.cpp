// The native frontend reuses Daytona's SDL_GPU renderer and shared Model2A machine.
#include "runtime/model2a_machine.h"
#include "app/gpu/gpu_renderer.h"
#include "app/gpu_device.h"
#include "app/segarally_touch.h"
#include "app/segarally_audio.h"
#include "app/segarally_audio_capture.h"
#include "../../tools/srally_boot/input_replay.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Machine=rt::Model2AMachine;
std::vector<uint8_t> read_file(const std::filesystem::path &path, bool optional=false) {
    std::ifstream file(path,std::ios::binary);
    if(!file) {if(optional) return {};throw std::runtime_error("Cannot read "+path.string());}
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void write_file(const std::filesystem::path &path,const std::vector<uint8_t> &bytes) {
    const auto temporary=path.string()+".tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));
     if(!file) throw std::runtime_error("Cannot save "+path.string());}
    std::filesystem::rename(temporary,path);
}
void save(Machine &machine,const std::filesystem::path &directory) {
    write_file(directory/"eeprom.bin",machine.save_eeprom());write_file(directory/"backup.bin",machine.save_backup());
}
void capture(SDL_GPUDevice *device,SDL_GPUTexture *texture,const std::filesystem::path &path) {
    constexpr Uint32 w=496,h=384;
    SDL_GPUTransferBufferCreateInfo ci{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*4,0};
    auto *buffer=SDL_CreateGPUTransferBuffer(device,&ci);
    if(!buffer) throw std::runtime_error(SDL_GetError());
    auto *cmd=SDL_AcquireGPUCommandBuffer(device);
    if(!cmd) {SDL_ReleaseGPUTransferBuffer(device,buffer);throw std::runtime_error(SDL_GetError());}
    auto *pass=SDL_BeginGPUCopyPass(cmd);
    if(!pass) {SDL_CancelGPUCommandBuffer(cmd);SDL_ReleaseGPUTransferBuffer(device,buffer);throw std::runtime_error(SDL_GetError());}
    SDL_GPUTextureRegion src{};src.texture=texture;src.w=w;src.h=h;src.d=1;
    SDL_GPUTextureTransferInfo dst{};dst.transfer_buffer=buffer;dst.pixels_per_row=w;dst.rows_per_layer=h;
    SDL_DownloadFromGPUTexture(pass,&src,&dst);SDL_EndGPUCopyPass(pass);
    auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if(!fence || !SDL_WaitForGPUFences(device,true,&fence,1)) {SDL_ReleaseGPUTransferBuffer(device,buffer);throw std::runtime_error(SDL_GetError());}
    auto *pixels=static_cast<const uint8_t *>(SDL_MapGPUTransferBuffer(device,buffer,false));
    if(!pixels) {SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,buffer);throw std::runtime_error(SDL_GetError());}
    std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"P6\n"<<w<<" "<<h<<"\n255\n";
    uint64_t hash=0xcbf29ce484222325ULL;
    for(size_t i=0;i<w*h*4;++i) {hash^=pixels[i];hash*=0x100000001b3ULL;}
    for(size_t i=0;i<w*h;++i) {const char rgb[]={char(pixels[4*i+2]),char(pixels[4*i+1]),char(pixels[4*i])};file.write(rgb,3);}
    SDL_UnmapGPUTransferBuffer(device,buffer);SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,buffer);
    if(!file) throw std::runtime_error("Cannot write GPU capture");
    fprintf(stderr,"Metal frame capture: %s BGRA hash=%016llx\n",path.string().c_str(),static_cast<unsigned long long>(hash));
}
void capture_reference(const rt::Video &video,const std::filesystem::path &path) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"P6\n496 384\n255\n";
    for(const auto pixel:video.screen()) {
        const char rgb[]={char(pixel>>16),char(pixel>>8),char(pixel)};file.write(rgb,3);
    }
    if(!file) throw std::runtime_error("Cannot write native software reference capture");
    fprintf(stderr,"Native software reference capture: %s BGRA hash=%016llx\n",path.string().c_str(),
        static_cast<unsigned long long>(video.screen_hash()));
}
} // namespace

int segarally_host_main(int argc,char **argv) {
    std::filesystem::path data,saves,capture_path,reference_path,log_path,replay_path,audio_path;
    uint64_t frame_limit=0,instruction_limit=0;
    bool touch_visible=false,unthrottled=false,hidden=false,fullscreen=false,sound=false;
    bool instruction_boundary_hook=false;
    if(const char *env=std::getenv("SEGARALLY_DATA_DIR")) data=env;
    if(const char *env=std::getenv("SEGARALLY_SAVE_DIR")) saves=env;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        auto value=[&](){if(++i>=argc) throw std::runtime_error("Missing argument for "+arg);return std::string(argv[i]);};
        if(arg=="--data") data=value();else if(arg=="--save-dir") saves=value();
        else if(arg=="--frames") frame_limit=std::stoull(value());else if(arg=="--instructions") instruction_limit=std::stoull(value());
        else if(arg=="--capture") capture_path=value();else if(arg=="--touch") touch_visible=true;
        else if(arg=="--reference-capture") reference_path=value();
        else if(arg=="--unthrottled") unthrottled=true;
        else if(arg=="--hidden") hidden=true;
        else if(arg=="--fullscreen") fullscreen=true;
        else if(arg=="--log") log_path=value();
        else if(arg=="--input-replay") replay_path=value();
        else if(arg=="--sound") sound=true;
        else if(arg=="--instruction-boundary-hook") instruction_boundary_hook=true;
        else if(arg=="--audio-capture") audio_path=value();
        else if(arg=="--help") {puts("Sega Rally native: --data DIR --save-dir DIR --frames N --instructions N --capture FILE.ppm --reference-capture FILE.ppm --touch --unthrottled --hidden --fullscreen --log FILE --input-replay FILE --sound --audio-capture FILE.wav --instruction-boundary-hook\nSound is opt-in and uses diagnostic instruction-boundary timing. Unthrottled runs capture/drain audio without device playback.\nKeys: arrows steer, Z gas, X brake, C coin, Return start, V view, A/S gear down/up, Space handbrake, F2 service, F3 test, Escape quit.");return 0;}
        else throw std::runtime_error("Unknown argument "+arg);
    }
    if(!audio_path.empty() && !sound) throw std::runtime_error("--audio-capture requires --sound");
#ifndef M2_SRALLY_SOUND
    if(sound) throw std::runtime_error("Build with SEGARALLY_SOUND=ON before using --sound");
#endif
#ifndef M2_INSTRUCTION_BOUNDARY_HOOK
    if(instruction_boundary_hook) throw std::runtime_error("Build with SEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON before using --instruction-boundary-hook");
#endif
    if(!log_path.empty()) {
        if(!freopen(log_path.string().c_str(),"w",stderr)) throw std::runtime_error("Cannot open native host log");
        setvbuf(stderr,nullptr,_IONBF,0);
    }
    fprintf(stderr,"Sega Rally native host startup\n");
    std::unique_ptr<srally::InputReplay> replay;
    if(!replay_path.empty()) {
        std::ifstream stream(replay_path);
        if(!stream) throw std::runtime_error("Cannot read input replay "+replay_path.string());
        replay=std::make_unique<srally::InputReplay>(stream);
        fprintf(stderr,"Input replay loaded: %zu events, completed-render frame boundaries\n",replay->size());
    }
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_GPU_DRIVER,"metal");
    if(hidden) SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
    if(!SDL_Init(SDL_INIT_VIDEO)) {fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;}
    if(data.empty()) data=std::filesystem::path(SDL_GetBasePath())/"assets";
    if(saves.empty()) {char *path=SDL_GetPrefPath("sega-rally-recomp","SegaRallyNative");if(path) {saves=path;SDL_free(path);}else saves="saves";}
    SDL_Window *window=nullptr;SDL_GPUDevice *device=nullptr;SDL_GPUTexture *screen=nullptr;
    app::GpuRenderer gpu;rally::TouchControls touch;std::unique_ptr<Machine> machine,reference;
    rally::AudioOutput audio;
    std::unique_ptr<rally::AudioCapture> audio_capture;
    uint64_t audio_frames=0,audio_nonzero=0;
    double audio_energy=0;float audio_peak=0;
    int result=0;
    try {
        std::filesystem::create_directories(saves);
        Machine::Images images;
        images.program=read_file(data/"program.bin");images.main_data=read_file(data/"main_data.bin");
        images.tgp_tables=read_file(data/"tgp_tables.bin");images.copro_data=read_file(data/"copro_data.bin");
        images.polygons=read_file(data/"polygons.bin");images.textures=read_file(data/"textures.bin");
        if(sound) {images.sound_program=read_file(data/"sound_program.bin");images.sound_samples=read_file(data/"sound_samples.bin");}
        Machine::Options options;options.external_video=true;options.sound=sound;
        options.instruction_boundary_hook=instruction_boundary_hook;
        if(!reference_path.empty()) {Machine::Options ref_options;ref_options.sound=sound;reference=std::make_unique<Machine>(images,ref_options);}
        machine=std::make_unique<Machine>(std::move(images),options);
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
        fprintf(stderr,"Exact instruction boundary hook=%d (reference comparison uses outer loop)\n",int(instruction_boundary_hook));
#endif
        static_assert(Machine::kAudioRate==rally::AudioOutput::kRate);
        if(sound && !unthrottled) audio.open();
        if(!audio_path.empty()) audio_capture=std::make_unique<rally::AudioCapture>(audio_path);
        const auto eeprom=read_file(saves/"eeprom.bin",true),backup=read_file(saves/"backup.bin",true);
        if(!eeprom.empty()) machine->load_eeprom(eeprom);if(!backup.empty()) machine->load_backup(backup);
        if(reference) {if(!eeprom.empty()) reference->load_eeprom(eeprom);if(!backup.empty()) reference->load_backup(backup);}
        SDL_WindowFlags flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if(hidden) flags|=SDL_WINDOW_HIDDEN;
        if(fullscreen) flags|=SDL_WINDOW_FULLSCREEN;
#ifdef SEGARALLY_IOS
        flags|=SDL_WINDOW_FULLSCREEN;touch_visible=true;
#endif
        window=SDL_CreateWindow("Sega Rally Native",992,768,flags);
        device=app::create_gpu_device();
        if(!window || !device || strcmp(SDL_GetGPUDeviceDriver(device),"metal") || !SDL_ClaimWindowForGPUDevice(device,window))
            throw std::runtime_error(std::string("Metal initialization: ")+SDL_GetError());
        SDL_SetGPUSwapchainParameters(device,window,SDL_GPU_SWAPCHAINCOMPOSITION_SDR,SDL_GPU_PRESENTMODE_VSYNC);
        fprintf(stderr,"GPU driver=%s device=%s native Model2A core, SCSP=%s playback=%d\n",SDL_GetGPUDeviceDriver(device),
            SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown"),
            sound?"instruction_boundary":"disabled",int(sound&&!unthrottled));
        SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        ti.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;ti.width=496;ti.height=384;ti.layer_count_or_depth=1;ti.num_levels=1;
        screen=SDL_CreateGPUTexture(device,&ti);
        if(!screen || !gpu.init(device,ti.format) || !touch.init(device,SDL_GetGPUSwapchainTextureFormat(device,window)))
            throw std::runtime_error(std::string("Metal pipelines: ")+gpu.error()+" "+SDL_GetError());
        touch.visible=touch_visible;touch.layout(window);
        bool quit=false,background=false,stopped=false;int gear=0;
        uint64_t replay_release_frame=0;bool replay_manual_takeover=false;
        Uint64 start=SDL_GetTicksNS();uint64_t displayed=0,last_frames=0;
        while(!quit) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_TERMINATING) quit=true;
                if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND) {background=true;touch.clear();audio.clear();save(*machine,saves);}
                if(event.type==SDL_EVENT_DID_ENTER_FOREGROUND) {background=false;audio.clear();start=SDL_GetTicksNS()-machine->stats().board_time_ns;}
                if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST) touch.clear();
                if(event.type==SDL_EVENT_WINDOW_RESIZED || event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type==SDL_EVENT_WINDOW_SAFE_AREA_CHANGED) touch.layout(window);
                touch.event(event);
            }
            if(background) {SDL_Delay(20);continue;}
            const bool *keys=SDL_GetKeyboardState(nullptr);
            if(keys[SDL_SCANCODE_ESCAPE]) quit=true;
            auto &in=machine->inputs();
            const auto before=machine->stats();
            if(replay && replay->apply(before.frames_rendered,in)) {
                fprintf(stderr,"Input replay event: frame=%llu instructions=%llu cycles=%llu in0=%02x steer=%02x gas=%02x brake=%02x\n",
                    static_cast<unsigned long long>(before.frames_rendered),static_cast<unsigned long long>(before.instructions),
                    static_cast<unsigned long long>(before.reference_cycles),in.in0,in.analog[0],in.analog[1],in.analog[2]);
                if(replay->applied()==replay->size()) replay_release_frame=before.frames_rendered+1;
            }
            const bool replay_active=replay && (replay->applied()<replay->size() || before.frames_rendered<replay_release_frame);
            if(replay && !replay_active && !replay_manual_takeover) {
                replay_manual_takeover=true;
                fprintf(stderr,"Input replay complete: manual takeover at frame=%llu\n",static_cast<unsigned long long>(before.frames_rendered));
            }
            if(!replay_active) {
                in={};
                auto low=[&](unsigned bit,bool held){if(held) in.in0&=uint8_t(~(1u<<bit));};
                low(0,keys[SDL_SCANCODE_C]||touch.held[rally::Coin]);low(6,keys[SDL_SCANCODE_RETURN]||touch.held[rally::Start]);
                low(5,keys[SDL_SCANCODE_V]||touch.held[rally::View]);low(2,keys[SDL_SCANCODE_F2]);low(3,keys[SDL_SCANCODE_F3]);
                static bool last_up=false,last_down=false;
                const bool up=keys[SDL_SCANCODE_S]||touch.held[rally::GearUp],down=keys[SDL_SCANCODE_A]||touch.held[rally::GearDown];
                if(up&&!last_up) gear=std::min(4,gear+1);if(down&&!last_down) gear=std::max(0,gear-1);last_up=up;last_down=down;
                constexpr uint8_t gears[]={0,0x20,0x10,0x60,0x50};in.in1=uint8_t(0x8f|gears[gear]);
                in.in2=(keys[SDL_SCANCODE_SPACE]||touch.held[rally::Handbrake])?0xff:0;
                const int steering=int(keys[SDL_SCANCODE_RIGHT]||touch.held[rally::Right])-int(keys[SDL_SCANCODE_LEFT]||touch.held[rally::Left]);
                in.analog[0]=steering<0?0:steering>0?255:128;
                in.analog[1]=(keys[SDL_SCANCODE_Z]||touch.held[rally::Gas])?255:0;
                in.analog[2]=(keys[SDL_SCANCODE_X]||touch.held[rally::Brake])?255:0;
            }
            bool new_frame=false;
            if(!stopped && !quit) {
                const uint64_t completed=machine->stats().instructions;
                if(instruction_limit && completed>=instruction_limit) break;
                const uint64_t budget=instruction_limit?std::min(uint64_t(100000),instruction_limit-completed):100000;
                const auto outcome=machine->run_until_frame(budget);
                if(reference) {
                    reference->inputs()=in;
                    const auto expected=reference->run_until_frame(budget);
                    const auto a=machine->stats(),b=reference->stats();
                    if(outcome.reason!=expected.reason || a.instructions!=b.instructions || a.pc!=b.pc ||
                       a.reference_cycles!=b.reference_cycles || a.frames_rendered!=b.frames_rendered)
                        throw std::runtime_error("Native software/Metal machines diverged before presentation");
                }
                if(outcome.reason==Machine::Reason::Frame) new_frame=true;
                else if(outcome.reason!=Machine::Reason::Budget) {
                    stopped=true;result=3;audio.clear();
                    fprintf(stderr,"Native frontier: pc=%08x instructions=%llu %s\n",outcome.pc,
                            static_cast<unsigned long long>(outcome.instructions),outcome.message.c_str());
                    SDL_SetWindowTitle(window,("Sega Rally Native - frontier: "+outcome.message).c_str());
                    if(frame_limit||instruction_limit) quit=true;
                }
            }
            if(sound) {
                const auto samples=machine->take_audio();
                if(reference && reference->take_audio()!=samples)
                    throw std::runtime_error("Native software/Metal machines produced different SCSP samples");
                audio_frames+=samples.size()/2;
                for(const float sample:samples) {
                    if(!std::isfinite(sample)) throw std::runtime_error("Non-finite SCSP output sample");
                    audio_nonzero+=sample!=0;audio_peak=std::max(audio_peak,std::abs(sample));
                    audio_energy+=double(sample)*sample;
                }
                if(audio_capture) audio_capture->push(samples);
                if(!unthrottled && !stopped) audio.push(samples);
            }
            if(instruction_limit && machine->stats().instructions>=instruction_limit) quit=true;
            if(quit && !new_frame) break;
            if(!new_frame && !stopped) continue;
            auto *cmd=SDL_AcquireGPUCommandBuffer(device);if(!cmd) throw std::runtime_error(SDL_GetError());
            SDL_GPUTexture *swapchain=nullptr;Uint32 sw=0,sh=0;
            if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd,window,&swapchain,&sw,&sh)) throw std::runtime_error(SDL_GetError());
            if(new_frame) {gpu.render(cmd,screen,496,384,machine->video());++displayed;}
            if(swapchain && displayed) {
                const float scale=std::min(sw/496.f,sh/384.f);const Uint32 dw=Uint32(496*scale),dh=Uint32(384*scale);
                SDL_GPUBlitInfo blit{};blit.source.texture=screen;blit.source.w=496;blit.source.h=384;
                blit.destination.texture=swapchain;blit.destination.x=(sw-dw)/2;blit.destination.y=(sh-dh)/2;blit.destination.w=dw;blit.destination.h=dh;
                blit.load_op=SDL_GPU_LOADOP_CLEAR;blit.clear_color={0,0,0,1};blit.filter=SDL_GPU_FILTER_LINEAR;
                SDL_BlitGPUTexture(cmd,&blit);touch.draw(cmd,swapchain);
            }
            if(!SDL_SubmitGPUCommandBuffer(cmd)) throw std::runtime_error(SDL_GetError());
            const auto stats=machine->stats();
            if(displayed/60!=last_frames/60) fprintf(stderr,"frames=%llu instructions=%llu cycles=%llu pc=%08x\n",
                static_cast<unsigned long long>(displayed),static_cast<unsigned long long>(stats.instructions),
                static_cast<unsigned long long>(stats.reference_cycles),stats.pc);
            last_frames=displayed;
            if((frame_limit&&displayed>=frame_limit)||(instruction_limit&&stats.instructions>=instruction_limit)) quit=true;
            if(!unthrottled && new_frame) {
                // Pace the host to the machine's exact board time, without changing device clocks.
                const Uint64 deadline=start+stats.board_time_ns,now=SDL_GetTicksNS();
                if(deadline>now) SDL_DelayNS(deadline-now);
            }
            if(stopped) SDL_Delay(20);
        }
        if(!capture_path.empty() && displayed) capture(device,screen,capture_path);
        if(reference && displayed) capture_reference(reference->video(),reference_path);
        save(*machine,saves);
        if(audio_capture) audio_capture->finish();
        if(replay) fprintf(stderr,"Input replay exit: events=%zu applied=%zu pending=%zu manual_takeover=%d\n",
            replay->size(),replay->applied(),replay->size()-replay->applied(),int(replay_manual_takeover));
        const auto stats=machine->stats();
        if(sound) fprintf(stderr,"Native audio: rate=%u frames=%llu nonzero_samples=%llu peak=%.9g rms=%.9g chip_dropped=%llu host_dropped=%llu queued=%u device_shortfall_requests=%llu requested_extra_input_bytes=%llu capture=%s\n",
            Machine::kAudioRate,static_cast<unsigned long long>(audio_frames),static_cast<unsigned long long>(audio_nonzero),audio_peak,
            audio_frames?std::sqrt(audio_energy/(2*audio_frames)):0,
            static_cast<unsigned long long>(stats.sound_dropped_samples),static_cast<unsigned long long>(audio.dropped_frames()),
            audio.queued_frames(),static_cast<unsigned long long>(audio.shortfall_requests()),
            static_cast<unsigned long long>(audio.requested_extra_bytes()),audio_path.empty()?"none":audio_path.string().c_str());
        fprintf(stderr,"Native host exit: frames=%llu instructions=%llu cycles=%llu pc=%08x result=%d\n",
            static_cast<unsigned long long>(displayed),static_cast<unsigned long long>(stats.instructions),
            static_cast<unsigned long long>(stats.reference_cycles),stats.pc,result);
    } catch(const std::exception &error) {fprintf(stderr,"Sega Rally native: %s\n",error.what());result=1;}
    audio.close();
    if(device) {SDL_WaitForGPUIdle(device);touch.shutdown();gpu.shutdown();if(screen) SDL_ReleaseGPUTexture(device,screen);
        if(window) SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);}
    if(window) SDL_DestroyWindow(window);SDL_Quit();return result;
}

#ifndef SEGARALLY_IOS
int main(int argc,char **argv) {
    try {return segarally_host_main(argc,argv);}catch(const std::exception &error) {fprintf(stderr,"Sega Rally native: %s\n",error.what());return 1;}
}
#endif
