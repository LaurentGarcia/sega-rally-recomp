// The native frontend reuses Daytona's SDL_GPU renderer and shared Model2A machine.
#include "runtime/model2a_machine.h"
#include "app/gpu/gpu_renderer.h"
#include "app/gpu/crt_renderer.h"
#include "app/gpu_device.h"
#include "app/segarally_display.h"
#include "app/segarally_controls.h"
#include "app/segarally_touch.h"
#include "app/segarally_audio.h"
#include "app/segarally_audio_capture.h"
#include "../../tools/srally_boot/input_replay.h"
#ifdef SEGARALLY_INPUT_RECORDING
#include "../../tools/srally_boot/recording_bundle.h"
#include <mach-o/dyld.h>
#include <optional>
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Machine=rt::Model2AMachine;
#ifdef SEGARALLY_INPUT_RECORDING
namespace record=srally::recording;
std::filesystem::path actual_executable() {
    std::array<char,4096> path{};uint32_t size=uint32_t(path.size());
    if(_NSGetExecutablePath(path.data(),&size)!=0)throw std::runtime_error("Cannot identify actual host executable");
    return std::filesystem::canonical(path.data());
}
std::filesystem::path replay_default_data(const std::filesystem::path &exe) {
    const auto parent=exe.parent_path();
    if(parent.filename()=="MacOS"&&parent.parent_path().filename()=="Contents")return parent.parent_path()/"Resources/assets";
    return parent/"assets";
}
#endif
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
    bool crt_requested=false,original_display=false,accurate_textures=false,show_display_controls=false;
    bool instruction_boundary_hook=false,audio_diagnostics=false;
#if defined(__APPLE__) && !defined(SEGARALLY_IOS)
    // Finder starts the dedicated app without arguments. Explicit diagnostic
    // commands retain their opt-in defaults, including offline replay.
    if(argc==1) {
#ifdef M2_SRALLY_SOUND
        sound=true;
#endif
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
        instruction_boundary_hook=true;
#endif
    }
#endif
#ifdef SEGARALLY_INPUT_RECORDING
    std::filesystem::path record_path,recording_replay_path;
    bool recording_replay_conflict=false;
#endif
    if(const char *env=std::getenv("SEGARALLY_DATA_DIR")) data=env;
    if(const char *env=std::getenv("SEGARALLY_SAVE_DIR")) saves=env;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
#ifdef SEGARALLY_INPUT_RECORDING
        if(arg!="--data"&&arg!="--replay-recording")recording_replay_conflict=true;
#endif
        auto value=[&](){if(++i>=argc) throw std::runtime_error("Missing argument for "+arg);return std::string(argv[i]);};
        if(arg=="--data") data=value();else if(arg=="--save-dir") {
            saves=value();
        }
        else if(arg=="--frames") frame_limit=std::stoull(value());else if(arg=="--instructions") instruction_limit=std::stoull(value());
        else if(arg=="--capture") capture_path=value();else if(arg=="--touch") touch_visible=true;
        else if(arg=="--reference-capture") reference_path=value();
        else if(arg=="--unthrottled") unthrottled=true;
        else if(arg=="--hidden") hidden=true;
        else if(arg=="--fullscreen") fullscreen=true;
        else if(arg=="--crt") crt_requested=true;
        else if(arg=="--original-display") original_display=true;
        else if(arg=="--accurate-textures") accurate_textures=true;
        else if(arg=="--display-controls") show_display_controls=true;
        else if(arg=="--log") log_path=value();
        else if(arg=="--input-replay") replay_path=value();
#ifdef SEGARALLY_INPUT_RECORDING
        else if(arg=="--record-inputs") record_path=value();
        else if(arg=="--replay-recording") recording_replay_path=value();
#endif
        else if(arg=="--sound") sound=true;
        else if(arg=="--audio-diagnostics") audio_diagnostics=true;
        else if(arg=="--instruction-boundary-hook") instruction_boundary_hook=true;
        else if(arg=="--audio-capture") audio_path=value();
        else if(arg=="--help") {puts("Sega Rally native: --data DIR --save-dir DIR --frames N --instructions N --capture FILE.ppm --reference-capture FILE.ppm --touch --unthrottled --hidden --fullscreen --log FILE --input-replay FILE --sound --audio-capture FILE.wav --audio-diagnostics --instruction-boundary-hook\nNo-argument Mac startup enables compiled sound and exact instruction-boundary execution. Explicit commands keep sound opt-in with diagnostic instruction-boundary timing. Unthrottled runs capture/drain audio without device playback.\nKeys: arrows steer, Z gas, X brake, C coin, Return start, V view, A/S gear down/up, Space handbrake, F2 test, F3 service, Escape quit.");
            puts("Display: F1 opens controls and pauses; F6 toggles CRT effects. --crt enables the Arcade CRT preset; --original-display uses display defaults; --accurate-textures enables optional native interpolation; --display-controls opens the panel. Native --capture remains before display effects.");
            puts("Controls: F1 or Xbox Y opens Settings. Configure keyboard/controller bindings and optional controller input in Controls. Xbox defaults: left stick steer, RT gas, LT brake, LB/RB shift, A view, X handbrake, Back coin, Start start.");
#ifdef SEGARALLY_INPUT_RECORDING
            puts("--record-inputs DIR creates a new bounded recording bundle. --replay-recording DIR [--data DIR] replays it offline with this exact executable; no window/audio device/live-save writes.");
#endif
            return 0;}
        else throw std::runtime_error("Unknown argument "+arg);
    }
#ifdef SEGARALLY_INPUT_RECORDING
    if(!recording_replay_path.empty()) {
        if(recording_replay_conflict)
            throw std::runtime_error("--replay-recording accepts only --data; it uses the recorded Machine options and startup saves");
        const auto exe=actual_executable();if(data.empty())data=replay_default_data(exe);
        const auto replay=record::replay_bundle(recording_replay_path,data,exe);
        printf("Same-Machine replay matched: resumes=%llu instructions=%llu cycles=%llu frames=%llu terminal=%s\n",
            static_cast<unsigned long long>(replay.records),static_cast<unsigned long long>(replay.terminal.instructions),
            static_cast<unsigned long long>(replay.terminal.cycles),static_cast<unsigned long long>(replay.terminal.frames),record::terminal_name(replay.kind));
        return 0;
    }
#endif
    if(audio_diagnostics && (!sound || unthrottled))
        throw std::runtime_error("--audio-diagnostics requires paced --sound playback");
    if(crt_requested && original_display) throw std::runtime_error("--crt and --original-display cannot be combined");
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
    app::CrtRenderer crt;app::DisplayPanel display;app::DisplaySettings display_settings;
    app::segarally::Controls controls;
    bool crt_ready=false;
    rally::AudioOutput audio;
    std::unique_ptr<rally::AudioCapture> audio_capture;
    uint64_t audio_frames=0,audio_nonzero=0;
    double audio_energy=0;float audio_peak=0;
    int result=0;
#ifdef SEGARALLY_INPUT_RECORDING
    std::unique_ptr<record::Recorder> recording;
    record::TerminalKind terminal_kind=record::TerminalKind::Quit;
#endif
    try {
        std::filesystem::create_directories(saves);
        Machine::Images images;
        images.program=read_file(data/"program.bin");images.main_data=read_file(data/"main_data.bin");
        images.tgp_tables=read_file(data/"tgp_tables.bin");images.copro_data=read_file(data/"copro_data.bin");
        images.polygons=read_file(data/"polygons.bin");images.textures=read_file(data/"textures.bin");
        if(sound) {images.sound_program=read_file(data/"sound_program.bin");images.sound_samples=read_file(data/"sound_samples.bin");}
        Machine::Options options;options.external_video=true;options.sound=sound;
        options.instruction_boundary_hook=instruction_boundary_hook;
#ifdef SEGARALLY_INPUT_RECORDING
        std::optional<record::StartupIdentity> recording_identity;
        if(!record_path.empty())recording_identity=record::identify_startup(images,options,
            record::read_bounded(data/"assets.json",1024*1024),actual_executable());
#endif
        if(!reference_path.empty()) {Machine::Options ref_options;ref_options.sound=sound;reference=std::make_unique<Machine>(images,ref_options);}
        machine=std::make_unique<Machine>(std::move(images),options);
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
        fprintf(stderr,"Exact instruction boundary hook=%d (reference comparison uses outer loop)\n",int(instruction_boundary_hook));
#endif
        static_assert(Machine::kAudioRate==rally::AudioOutput::kRate);
        if(!audio_path.empty()) audio_capture=std::make_unique<rally::AudioCapture>(audio_path);
        const auto eeprom=read_file(saves/"eeprom.bin",true),backup=read_file(saves/"backup.bin",true);
        if(!eeprom.empty()) machine->load_eeprom(eeprom);if(!backup.empty()) machine->load_backup(backup);
        if(reference) {if(!eeprom.empty()) reference->load_eeprom(eeprom);if(!backup.empty()) reference->load_backup(backup);}
#ifdef SEGARALLY_INPUT_RECORDING
        if(recording_identity)recording=std::make_unique<record::Recorder>(record_path,*recording_identity,*machine);
#endif
        SDL_WindowFlags flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if(hidden) flags|=SDL_WINDOW_HIDDEN;
        if(fullscreen) flags|=SDL_WINDOW_FULLSCREEN;
#ifdef SEGARALLY_IOS
        flags|=SDL_WINDOW_FULLSCREEN;touch_visible=true;
#endif
        window=SDL_CreateWindow("Sega Rally Native | F1 / Y Settings | F6 CRT",992,768,flags);
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
        if(!display.init(window,device,SDL_GetGPUSwapchainTextureFormat(device,window),saves,display_settings))
            throw std::runtime_error(display.error());
        if(!display.error().empty()) fprintf(stderr,"Display preferences: %s\n",display.error().c_str());
        if(!controls.init(saves)) fprintf(stderr,"Controller initialization: %s; keyboard remains available\n",controls.error().c_str());
        else if(!controls.error().empty()) fprintf(stderr,"Control preferences: %s\n",controls.error().c_str());
        display.attach_controls(&controls);
        fprintf(stderr,"Controller: %s enabled=%d; F1 or Y opens Settings\n",controls.device_name().c_str(),int(controls.mapper.settings.gamepad_enabled));
        if(original_display) display_settings=app::DisplaySettings{};
        else if(const char *env=std::getenv("SEGARALLY_GPU_POLYGON_SCANLINES"))
            display_settings.accurate_textures=std::strcmp(env,"1")==0;
        if(crt_requested) {display_settings.crt=app::CrtSettings{};display_settings.crt.enabled=true;}
        if(accurate_textures) display_settings.accurate_textures=true;
        gpu.set_polygon_scanlines(display_settings.accurate_textures);
        bool rendered_accurate_textures=display_settings.accurate_textures;
        fprintf(stderr,"Display controls: F1 opens panel; F6 toggles CRT; effects=%d accurate_textures=%d\n",
            int(display_settings.crt.enabled),int(display_settings.accurate_textures));
        audio.enable_diagnostics(audio_diagnostics);
        if(sound && !unthrottled) audio.open();
        bool quit=false,background=false,stopped=false,display_paused=false,focused=true;
        uint64_t replay_release_frame=0;bool replay_manual_takeover=false;
        Uint64 start=SDL_GetTicksNS();uint64_t displayed=0,last_frames=0;
        // These wall-time observations do not alter board clocks or PCM. GPU
        // submission/presentation wall time includes host waiting, not GPU time.
        const Uint64 diagnostic_origin=audio_diagnostics?SDL_GetTicksNS():0;
        Uint64 producer_at=0,max_producer_gap=0,max_step=0,max_presentation=0,max_lateness=0;
        uint64_t diagnostic_requests=0,diagnostic_period=0;
        const auto diagnostic_log=[&](bool final=false) {
            if(!audio_diagnostics) return;
            const auto demand=audio.demand_snapshot();
            const auto stats=machine->stats();
            const bool periodic=stats.frames_rendered/60!=diagnostic_period;
            const bool shortfall=demand.requests!=diagnostic_requests;
            if(!final && !periodic && !shortfall) return;
            const Uint64 now=SDL_GetTicksNS();
            fprintf(stderr,"Audio diagnostics: reason=%s frame=%llu board_ns=%llu host_elapsed_ns=%llu queued_frames=%u started=%d received_frames=%llu submitted_frames=%llu dropped_frames=%llu shortfall_requests=%llu estimated_extra_input_bytes=%llu last_shortfall_host_ns=%llu max_producer_gap_ns=%llu max_machine_step_ns=%llu max_presentation_wall_ns=%llu max_pacing_lateness_ns=%llu\n",
                final?"final":shortfall?"shortfall":"periodic",
                static_cast<unsigned long long>(stats.frames_rendered),static_cast<unsigned long long>(stats.board_time_ns),
                static_cast<unsigned long long>(now-diagnostic_origin),audio.queued_frames(),int(audio.started()),
                static_cast<unsigned long long>(audio.received_frames()),static_cast<unsigned long long>(audio.submitted_frames()),
                static_cast<unsigned long long>(audio.dropped_frames()),static_cast<unsigned long long>(demand.requests),
                static_cast<unsigned long long>(demand.extra_input_bytes),
                static_cast<unsigned long long>(demand.last_shortfall_ns>=diagnostic_origin?demand.last_shortfall_ns-diagnostic_origin:0),
                static_cast<unsigned long long>(max_producer_gap),static_cast<unsigned long long>(max_step),
                static_cast<unsigned long long>(max_presentation),static_cast<unsigned long long>(max_lateness));
            diagnostic_period=stats.frames_rendered/60;diagnostic_requests=demand.requests;
            max_producer_gap=max_step=max_presentation=max_lateness=0;
        };
        while(!quit) {
#ifdef SEGARALLY_INPUT_RECORDING
            std::optional<srally::host_input::Resume> recorded_resume;
            Machine::RunResult recorded_result;
#endif
            SDL_Event event;
            bool input_transition=false;
            while(SDL_PollEvent(&event)) {
                const bool controls_consumed=controls.process_event(event);
                display.process_event(event,controls_consumed);
                if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_TERMINATING) quit=true;
                if(!controls_consumed && event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if(event.key.scancode==SDL_SCANCODE_F1) display.set_open(!display.open());
                    else if(event.key.scancode==SDL_SCANCODE_F6) {
                        display_settings.crt.enabled=!display_settings.crt.enabled;
                        if(!display.save(display_settings)) fprintf(stderr,"Display preferences: %s\n",display.error().c_str());
                    } else if(event.key.scancode==SDL_SCANCODE_ESCAPE) {
                        if(display.open()) display.set_open(false);else quit=true;
                    }
                }
                if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND) {background=true;input_transition=true;touch.clear();audio.clear();save(*machine,saves);}
                if(event.type==SDL_EVENT_DID_ENTER_FOREGROUND) {background=false;input_transition=true;audio.clear();start=SDL_GetTicksNS()-machine->stats().board_time_ns;}
                if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST) {focused=false;input_transition=true;touch.clear();}
                if(event.type==SDL_EVENT_WINDOW_FOCUS_GAINED) {focused=true;input_transition=true;}
                if(event.type==SDL_EVENT_WINDOW_RESIZED || event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type==SDL_EVENT_WINDOW_SAFE_AREA_CHANGED) touch.layout(window);
                if(!display.open() && focused && !background) touch.event(event);
            }
            controls.poll();
            if(input_transition) controls.suppress_held();
            if(controls.take_menu_request() && focused && !background) display.set_open(!display.open());
            if(background) {controls.sample({},false);producer_at=0;SDL_Delay(20);continue;}
            if(display_paused!=display.open()) {
                display_paused=display.open();touch.clear();audio.clear();producer_at=0;controls.suppress_held();
                if(!display_paused) {
                    start=SDL_GetTicksNS()-machine->stats().board_time_ns;
                }
            }
            std::array<bool,app::segarally::action_count> touch_actions{};
            using Action=app::segarally::Action;
            for(const auto &[action,button]:std::array<std::pair<Action,rally::Button>,10>{{
                {Action::Left,rally::Left},{Action::Right,rally::Right},{Action::Gas,rally::Gas},
                {Action::Brake,rally::Brake},{Action::Coin,rally::Coin},{Action::Start,rally::Start},
                {Action::View,rally::View},{Action::GearDown,rally::GearDown},
                {Action::GearUp,rally::GearUp},{Action::Handbrake,rally::Handbrake}}})
                touch_actions[size_t(action)]=touch.held[size_t(button)];
            auto &in=machine->inputs();
            const auto before=machine->stats();
            // Display navigation cannot advance replay events or change the
            // remembered shifter while guest execution is paused.
            if(!display_paused) {
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
                in=controls.sample(touch_actions,focused);
            } else controls.sample(touch_actions,false);
            } else controls.sample(touch_actions,false);
            bool new_frame=false;
            if(!stopped && !quit && !display_paused) {
                const uint64_t completed=machine->stats().instructions;
                if(instruction_limit && completed>=instruction_limit) {
#ifdef SEGARALLY_INPUT_RECORDING
                    terminal_kind=record::TerminalKind::InstructionLimit;
#endif
                    break;
                }
                const uint64_t budget=instruction_limit?std::min(uint64_t(100000),instruction_limit-completed):100000;
#ifdef SEGARALLY_INPUT_RECORDING
                if(recording){srally::host_input::Resume row;row.index=recording->records();row.budget=budget;
                    row.before=srally::host_input::Witness::of(machine->stats());row.inputs=in;recorded_resume=row;}
#endif
                const Uint64 step_start=audio_diagnostics?SDL_GetTicksNS():0;
                const auto outcome=machine->run_until_frame(budget);
                if(audio_diagnostics) max_step=std::max(max_step,SDL_GetTicksNS()-step_start);
#ifdef SEGARALLY_INPUT_RECORDING
                if(recorded_resume){recorded_resume->after=srally::host_input::Witness::of(machine->stats());recorded_resume->reason=outcome.reason;recorded_result=outcome;}
#endif
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
#ifdef SEGARALLY_INPUT_RECORDING
                    terminal_kind=outcome.reason==Machine::Reason::RuntimeError?record::TerminalKind::Fault:record::TerminalKind::Frontier;
#endif
                    fprintf(stderr,"Native frontier: pc=%08x instructions=%llu %s\n",outcome.pc,
                            static_cast<unsigned long long>(outcome.instructions),outcome.message.c_str());
                    SDL_SetWindowTitle(window,("Sega Rally Native - frontier: "+outcome.message).c_str());
                    if(frame_limit||instruction_limit) quit=true;
                }
            }
            if(sound && !display_paused) {
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
                if(!unthrottled && !stopped) {
                    if(audio_diagnostics && !samples.empty()) {
                        const Uint64 now=SDL_GetTicksNS();
                        if(producer_at) max_producer_gap=std::max(max_producer_gap,now-producer_at);
                        producer_at=now;
                    }
                    audio.push(samples);
                }
#ifdef SEGARALLY_INPUT_RECORDING
                if(recorded_resume)recording->append(*recorded_resume,recorded_result,samples);
#endif
            }
#ifdef SEGARALLY_INPUT_RECORDING
            if(recorded_resume&&!sound)recording->append(*recorded_resume,recorded_result,{});
#endif
            if(instruction_limit && machine->stats().instructions>=instruction_limit) {
                quit=true;
#ifdef SEGARALLY_INPUT_RECORDING
                if(!stopped)terminal_kind=record::TerminalKind::InstructionLimit;
#endif
            }
            if(quit && !new_frame) break;
            if(!new_frame && !stopped && !display_paused) {diagnostic_log();continue;}
            const Uint64 presentation_start=audio_diagnostics?SDL_GetTicksNS():0;
            if(show_display_controls && new_frame) {display.set_open(true);show_display_controls=false;}
            if(!display.new_frame(display_settings)) throw std::runtime_error(display.error());
            auto *cmd=SDL_AcquireGPUCommandBuffer(device);if(!cmd) throw std::runtime_error(SDL_GetError());
            display.prepare(cmd);
            SDL_GPUTexture *swapchain=nullptr;Uint32 sw=0,sh=0;
            if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd,window,&swapchain,&sw,&sh)) throw std::runtime_error(SDL_GetError());
            const bool texture_mode_changed=rendered_accurate_textures!=display_settings.accurate_textures;
            if(new_frame || (displayed && texture_mode_changed)) {
                gpu.set_polygon_scanlines(display_settings.accurate_textures);
                gpu.render(cmd,screen,496,384,machine->video());
                rendered_accurate_textures=display_settings.accurate_textures;
                if(new_frame) ++displayed;
            }
            if(swapchain && displayed) {
                float scale=std::min(sw/496.f,sh/384.f);
                if(display_settings.integer_scale && scale>=1) scale=std::floor(scale);
                const Uint32 dw=std::max(1u,Uint32(496*scale)),dh=std::max(1u,Uint32(384*scale));
                if(display_settings.crt.enabled) {
                    if(!crt_ready) {
                        if(!crt.init(device,SDL_GetGPUSwapchainTextureFormat(device,window))) {
                            SDL_CancelGPUCommandBuffer(cmd);throw std::runtime_error(crt.error());
                        }
                        crt_ready=true;
                        fprintf(stderr,"Display CRT pipeline initialized for swapchain presentation\n");
                    }
                    const SDL_Rect viewport{int((sw-dw)/2),int((sh-dh)/2),int(dw),int(dh)};
                    if(!crt.render(cmd,screen,swapchain,viewport,display_settings.crt)) {
                        SDL_CancelGPUCommandBuffer(cmd);throw std::runtime_error(crt.error());
                    }
                } else {
                    SDL_GPUBlitInfo blit{};blit.source.texture=screen;blit.source.w=496;blit.source.h=384;
                    blit.destination.texture=swapchain;blit.destination.x=(sw-dw)/2;blit.destination.y=(sh-dh)/2;blit.destination.w=dw;blit.destination.h=dh;
                    blit.load_op=SDL_GPU_LOADOP_CLEAR;blit.clear_color={0,0,0,1};
                    blit.filter=display_settings.linear_filter?SDL_GPU_FILTER_LINEAR:SDL_GPU_FILTER_NEAREST;
                    SDL_BlitGPUTexture(cmd,&blit);
                }
                touch.draw(cmd,swapchain);
            }
            if(swapchain && display.open()) {
                SDL_GPUColorTargetInfo target{};target.texture=swapchain;
                target.load_op=displayed?SDL_GPU_LOADOP_LOAD:SDL_GPU_LOADOP_CLEAR;
                target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={0,0,0,1};
                auto *pass=SDL_BeginGPURenderPass(cmd,&target,1,nullptr);
                if(!pass) throw std::runtime_error(SDL_GetError());
                display.render(cmd,pass);SDL_EndGPURenderPass(pass);
            }
            if(!SDL_SubmitGPUCommandBuffer(cmd)) throw std::runtime_error(SDL_GetError());
            if(audio_diagnostics) max_presentation=std::max(max_presentation,SDL_GetTicksNS()-presentation_start);
            const auto stats=machine->stats();
            if(displayed/60!=last_frames/60) fprintf(stderr,"frames=%llu instructions=%llu cycles=%llu pc=%08x\n",
                static_cast<unsigned long long>(displayed),static_cast<unsigned long long>(stats.instructions),
                static_cast<unsigned long long>(stats.reference_cycles),stats.pc);
            last_frames=displayed;
            if((frame_limit&&displayed>=frame_limit)||(instruction_limit&&stats.instructions>=instruction_limit)) {
                quit=true;
#ifdef SEGARALLY_INPUT_RECORDING
                if(!stopped)terminal_kind=(frame_limit&&displayed>=frame_limit)?record::TerminalKind::FrameLimit:record::TerminalKind::InstructionLimit;
#endif
            }
            if(!unthrottled && new_frame) {
                // Pace the host to the machine's exact board time, without changing device clocks.
                const Uint64 deadline=start+stats.board_time_ns,now=SDL_GetTicksNS();
                if(audio_diagnostics && now>deadline) max_lateness=std::max(max_lateness,now-deadline);
                if(deadline>now) SDL_DelayNS(deadline-now);
            }
            diagnostic_log();
            if(stopped || display_paused) SDL_Delay(10);
        }
        diagnostic_log(true);
        if(!capture_path.empty() && displayed) capture(device,screen,capture_path);
        if(reference && displayed) capture_reference(reference->video(),reference_path);
        save(*machine,saves);
        if(audio_capture) audio_capture->finish();
#ifdef SEGARALLY_INPUT_RECORDING
        if(recording){recording->finish(*machine,terminal_kind);fprintf(stderr,"Recording complete: %s terminal=%s; same-Machine reproducibility only\n",record_path.string().c_str(),record::terminal_name(terminal_kind));}
#endif
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
    if(device) {SDL_WaitForGPUIdle(device);display.shutdown();crt.shutdown();touch.shutdown();gpu.shutdown();if(screen) SDL_ReleaseGPUTexture(device,screen);
        if(window) SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);}
    controls.shutdown();
    if(window) SDL_DestroyWindow(window);SDL_Quit();return result;
}

#ifndef SEGARALLY_IOS
int main(int argc,char **argv) {
    try {return segarally_host_main(argc,argv);}catch(const std::exception &error) {fprintf(stderr,"Sega Rally native: %s\n",error.what());return 1;}
}
#endif
