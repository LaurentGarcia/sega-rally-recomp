// Actual generated i960/TGP machine with paged images. CPU reference rendering
// is the default; native nearest-texture GS presentation is an explicit option.
#include "platform/ps2/game/rom_cache.h"
#include "runtime/model2a_machine.h"
#include "runtime/video.h"
#include "app/gpu/model2_frame.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#ifdef RALLY_OFFLINE_MESH_CAPTURE
#ifdef RALLY_PS2_EE
#error "Object mesh capture is an offline host operation"
#endif
#include "platform/ps2/tools/mesh_capture.h"
#include <charconv>
#endif
#ifdef RALLY_PS2_EE
#include "gs_display.h"
#include "pad_input.h"
#include "controls.h"
#include "storage.h"
#include "status_screen.h"
#include "debug_flow.h"
#include "launch_options.h"
#include "debug_controls.h"
#ifdef M2_SRALLY_SOUND
#include "audio_output.h"
#endif
#include <kernel.h>
#include <malloc.h>
#include <sifrpc.h>
#include <timer.h>
#else
#include "platform/ps2/include/gs_scene.h"
#include <fstream>
#include <map>
#endif

#ifndef RALLY_PS2_EE
static std::vector<uint8_t> load_image(const std::string &root, const char *name) {
    std::ifstream file(root + "/" + name + ".bin", std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("cannot open resident comparison image");
    const auto size = file.tellg();
    if (size < 0 || size > 0x2400000) throw std::runtime_error("invalid resident image size");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), size))
        throw std::runtime_error("cannot read resident comparison image");
    return bytes;
}

static void report_packets(const rt::Video &video, unsigned frame) {
    app::Model2Frame packets;
    packets.set_polygon_scanlines(false);
    packets.build(video, video.width(), rt::Video::H, 1);
    std::map<uint32_t, size_t> flags, dimensions;
    size_t textured = 0, checker = 0, mirror = 0, micro = 0;
    for (size_t i = 0; i < packets.vertices().size(); i += 3) {
        const auto &v = packets.vertices()[i];
        ++flags[v.info[0]];
        if ((v.info[0] & 3u) >= 2) {
            ++textured;
            ++dimensions[((v.info[0] >> 17) & 7u) | (((v.info[0] >> 20) & 7u) << 8)];
        }
        checker += bool(v.info[0] & 4u);
        mirror += bool(v.info[0] & 0x60u);
        micro += bool(v.info[0] & 0x80u);
    }
    std::fprintf(stderr, "PACKETS frame=%u polygons=%lu triangles=%lu batches=%lu textured=%lu checker=%lu mirror=%lu micro=%lu\n",
        frame, static_cast<unsigned long>(video.gpu_polys().size()),
        static_cast<unsigned long>(packets.vertices().size() / 3),
        static_cast<unsigned long>(packets.batches().size()),
        static_cast<unsigned long>(textured), static_cast<unsigned long>(checker),
        static_cast<unsigned long>(mirror), static_cast<unsigned long>(micro));
    for (const auto &[value, count] : flags)
        std::fprintf(stderr, "PACKET_FLAGS frame=%u bits=%08x triangles=%lu\n", frame,
            unsigned(value), static_cast<unsigned long>(count));
    for (const auto &[value, count] : dimensions)
        std::fprintf(stderr, "PACKET_SIZE frame=%u width=%u height=%u triangles=%lu\n", frame,
            32u << (value & 7u), 32u << ((value >> 8) & 7u), static_cast<unsigned long>(count));
}
#endif

#ifdef RALLY_PS2_EE
static void report_memory(const char *stage) {
    const auto info = mallinfo();
    std::printf("EE heap stage=%s arena=%lu allocated=%lu free_in_arena=%lu heap_end=%p\n",
        stage, static_cast<unsigned long>(info.arena), static_cast<unsigned long>(info.uordblks),
        static_cast<unsigned long>(info.fordblks), EndOfHeap());
}
#endif

static rt::Model2AMachine::RunResult run_frame(rt::Model2AMachine &machine
#if defined(RALLY_PS2_EE) && defined(M2_SRALLY_SOUND)
    , ::ps2::AudioOutput *audio
#endif
) {
    // Bounded slices retain instruction-boundary timing. The total per-frame
    // guard is unchanged; profiling never alters the guest cycle budget.
    constexpr unsigned total_budget = 50000000, slice_budget = 4096;
    rt::Model2AMachine::RunResult run;
    for (unsigned remaining = total_budget; remaining;) {
        const unsigned budget = std::min(remaining, slice_budget);
        run = machine.run_until_frame(budget);
#if defined(RALLY_PS2_EE) && defined(M2_SRALLY_SOUND)
        // Never call IOP RPC from a guest board-clock callback. Drain bounded
        // slices, including the final Frame result, into the device queue.
        if (audio && !audio->submit(machine.take_audio()))
            throw std::runtime_error(audio->error());
#elif defined(M2_SRALLY_SOUND)
        // Host checkpoints use the same bounded sound capture cadence.
        (void)machine.take_audio();
#endif
        if (run.reason != rt::Model2AMachine::Reason::Budget) return run;
        remaining -= budget;
    }
    return run;
}

int main(int argc, char **argv) {
#ifdef RALLY_PS2_EE
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    ::ps2::GsDisplay display;
    ::ps2::FlowDiagram diagram;
    bool display_ready = false;
#endif
    try {
#ifdef RALLY_PS2_EE
        ::ps2::PadInput pad;
        ::ps2::CabinetControls controls;
        if (!display.initialize()) throw std::runtime_error(display.error());
        display_ready = true;
        ::ps2::show_status(display, "GS READY\nINITIALIZING CONTROLLER AND STORAGE");
        // Detect a loader-provided host filesystem before any IOP reset can
        // discard its modules. USB startup can reset first and load its own.
        SifInitRpc(0);
        const char *path = argc > 1 ? argv[1] : "mass:/game.rpk";
        // PS2 development loaders and HLE test environments can provide host:
        // without a USB stack. An existing host pack selects that transport;
        // otherwise the normal console path initializes the open USB modules.
        bool host_transport = std::string(path).rfind("host:", 0) == 0;
        if (argc <= 1) {
            if (FILE *probe = std::fopen("host:game.rpk", "rb")) {
                std::fclose(probe);
                path = "host:game.rpk";
                host_transport = true;
            }
        }
        if (!pad.initialize(!host_transport))
            std::fprintf(stderr, "controller initialization: %s\n", pad.status());
        if (!host_transport && !::ps2::initialize_storage())
            throw std::runtime_error(::ps2::storage_error());
        if (!::ps2::wait_for_file(path)) throw std::runtime_error("asset pack not found on PS2 storage");
        std::printf("PS2 game pack: %s\n", path);
        ::ps2::LaunchOptions launch;
        std::string config_path(path);
        const auto separator = config_path.find_last_of("/:");
        config_path = separator == std::string::npos ? "rally.cfg" : config_path.substr(0, separator + 1) + "rally.cfg";
        char config_error[128]{};
        if (!::ps2::read_launch_options(config_path.c_str(), launch, config_error, sizeof(config_error)))
            throw std::runtime_error(config_error);
#ifndef M2_SRALLY_SOUND
        if (launch.audio) throw std::runtime_error("AUDIO REQUIRES A WITH-SOUND BUILD");
#endif
        display.set_vu0_packing(launch.vu0);
        bool debug_visible = launch.debug;
        ::ps2::DebugPageToggle debug_toggle;
        ::ps2::FlowSample flow;
        flow.frequency = kBUSCLK;
        flow.vu0 = launch.vu0;
        flow.renderer = launch.native_gs ? ::ps2::FlowSample::Renderer::Native : ::ps2::FlowSample::Renderer::Cpu;
        diagram.update(flow);
        ::ps2::show_status(display, "STORAGE READY\nOPENING GAME PACK");
        const unsigned frame_limit = 0;
#else
        if (argc < 2) {
            std::fprintf(stderr, "usage: core-probe PACK [frames=600] [resident-assets]\n");
            return 2;
        }
        const char *path = argv[1];
        const unsigned frame_limit = argc > 2 ? unsigned(std::strtoul(argv[2], nullptr, 10)) : 600;
        if (!frame_limit) throw std::runtime_error("host probe requires a positive frame limit");
        const bool packet_audit = std::getenv("RALLY_PS2_PACKET_AUDIT") != nullptr;
        const bool compact_reference = std::getenv("RALLY_PS2_COMPACT_REFERENCE") != nullptr;
        const char *scene_directory = std::getenv("RALLY_PS2_SCENE_DIRECTORY");
#ifdef RALLY_OFFLINE_MESH_CAPTURE
        const char *mesh_directory = std::getenv("RALLY_PS2_MESH_DIRECTORY");
        unsigned mesh_frame = 1634;
        if (const char *requested = std::getenv("RALLY_PS2_MESH_FRAME")) {
            const std::string text(requested);
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), mesh_frame);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !mesh_frame)
                throw std::runtime_error("mesh capture frame must be a positive integer");
        }
        if (mesh_directory && (!*mesh_directory || mesh_frame > frame_limit))
            throw std::runtime_error("mesh capture needs an output directory and a frame within the run");
#endif
#endif
        // Keep the first EE probe's cache bounded at 512 KiB. The pack reader
        // supports larger caches once executable/rendering peaks are measured.
#ifdef RALLY_PS2_EE
        auto cache = std::make_unique<rally::ps2::RomCache>(path, 128,
            [](const char *message, void *context) {
                std::puts(message);
                ::ps2::show_status(*static_cast<::ps2::GsDisplay *>(context), message);
            }, &display);
        ::ps2::show_status(display, "PACK CACHE READY\nREADING TGP TABLES");
#else
        auto cache = std::make_unique<rally::ps2::RomCache>(path, 128);
#endif
        rt::Model2AMachine::Images images;
        images.rom = cache.get();
        images.tgp_tables = cache->tgp_tables();
#ifdef RALLY_PS2_EE
        std::puts("TGP tables loaded");
#endif
#ifndef RALLY_PS2_EE
        if (argc > 3) {
            const std::string dir = argv[3];
            images.rom = nullptr;
            images.program = load_image(dir, "program");
            images.main_data = load_image(dir, "main_data");
            images.copro_data = load_image(dir, "copro_data");
            images.polygons = load_image(dir, "polygons");
            images.textures = load_image(dir, "textures");
        }
#endif
        rt::Model2AMachine::Options options;
        options.sound = false;
#if !defined(RALLY_PS2_EE) && defined(M2_SRALLY_SOUND)
        options.sound = std::getenv("RALLY_PS2_HOST_SOUND") != nullptr;
        options.sound_buffer_frames = 1024;
        if (options.sound) images.sound_rom = cache.get();
#endif
#if defined(RALLY_PS2_EE) && defined(M2_SRALLY_SOUND)
        options.sound = launch.audio;
        options.sound_buffer_frames = 1024;
        if (launch.audio) images.sound_rom = cache.get();
#endif
        options.instruction_boundary_hook = true;
#ifdef RALLY_PS2_EE
#ifdef M2_SRALLY_SOUND
        std::unique_ptr<::ps2::AudioOutput> audio;
        if (launch.audio) {
            audio = std::make_unique<::ps2::AudioOutput>();
            if (!audio->initialize()) throw std::runtime_error(audio->error());
            flow.sound = flow.sound_output = true;
        }
#endif
        ::ps2::show_status(display, "PACK OPEN\nALLOCATING MODEL 2 MACHINE");
#endif
        rt::Model2AMachine machine(std::move(images), options);
#ifndef RALLY_PS2_EE
        if (compact_reference) machine.video().set_compact_external_3d(true);
#endif
#ifdef RALLY_PS2_EE
        if (launch.native_gs) machine.video().set_compact_external_3d(true);
        app::Model2Frame packets;
        packets.set_polygon_scanlines(false);
        report_memory("machine-ready");
        ::ps2::show_status(display, "MACHINE READY\nRUNNING TRANSLATED GAME PROGRAM");
        // SDK system time is in EE bus-clock ticks. It measures work only and
        // never advances the Model 2 board clock. Emulator timings are separate
        // from physical-console performance measurements.
        machine.video().set_profile_clock([]() -> uint64_t {
            return GetTimerSystemTime();
        });
        std::printf("PROFILE clock=ee_bus_ticks frequency=%u\n", unsigned(kBUSCLK));
        bool game_visible = false;
        display.set_debug_page(debug_visible ? diagram.text() : nullptr);
        uint64_t previous_misses = cache->misses();
#endif
        std::puts("frame,instructions,cycles,board_ns,pc,register_hash,screen_hash");
        for (unsigned frame = 1; !frame_limit || frame <= frame_limit; ++frame) {
#ifdef RALLY_PS2_EE
            const uint64_t frame_started = GetTimerSystemTime();
            auto pad_state = pad.poll();
            const bool debug_toggled = debug_toggle.update(pad_state);
            if (debug_toggled) debug_visible = !debug_visible;
            display.set_debug_page(debug_visible ? diagram.text() : nullptr);
            machine.inputs() = controls.sample(pad_state);
            const uint64_t run_started = GetTimerSystemTime();
#endif
#ifdef RALLY_OFFLINE_MESH_CAPTURE
            if (mesh_directory && frame == mesh_frame)
                ::ps2::offline::begin_mesh_capture(mesh_directory, frame);
#endif
            const auto run = run_frame(machine
#if defined(RALLY_PS2_EE) && defined(M2_SRALLY_SOUND)
                , audio.get()
#endif
            );
#ifdef RALLY_PS2_EE
#ifdef M2_SRALLY_SOUND
            if (audio) {
                if (!audio->flush()) throw std::runtime_error(audio->error());
                const auto audio_stats = audio->stats();
                if (*audio->error()) throw std::runtime_error(audio->error());
                flow.queued_frames = audio_stats.queued;
                flow.silent_frames = audio_stats.silent_frames;
            }
#endif
            const uint64_t run_finished = GetTimerSystemTime();
            if (frame == 1 || frame == 60) report_memory("frame-complete");
#endif
            if (run.reason != rt::Model2AMachine::Reason::Frame) {
                std::fprintf(stderr, "core stopped at %08x: %s (reason %u)\n",
                             unsigned(run.pc), run.message.c_str(), unsigned(run.reason));
                char message[256];
                std::snprintf(message, sizeof(message), "CORE STOPPED AT %08X\nREASON %u\n%s",
                              unsigned(run.pc), unsigned(run.reason), run.message.c_str());
                throw std::runtime_error(message);
            }
#ifndef RALLY_PS2_EE
#ifdef RALLY_OFFLINE_MESH_CAPTURE
            if (mesh_directory && frame == mesh_frame)
                ::ps2::offline::end_mesh_capture();
#endif
            if (compact_reference) machine.video().render_compact_reference();
            if (scene_directory && (frame == 1357 || frame == 1634)) {
                std::string error;
                const auto scene_path = std::string(scene_directory) + "/frame-" + std::to_string(frame) + ".bin";
                if (!::ps2::capture_gs_scene(scene_path.c_str(), frame, machine.video(), error))
                    throw std::runtime_error(error);
                std::fprintf(stderr, "SCENE frame=%u path=%s\n", frame, scene_path.c_str());
            }
            if (packet_audit && (frame == 84 || frame == 600 || frame == 1357 || frame == 1634))
                report_packets(machine.video(), frame);
#endif
#ifdef RALLY_PS2_EE
            auto video_profile = machine.video().last_profile();
            bool native_presented = false;
            uint64_t packet_ticks = 0, fallback_ticks = 0;
            if (launch.native_gs) {
                const uint64_t packet_started = GetTimerSystemTime();
                packets.build(machine.video(), machine.video().width(), rt::Video::H, 1);
                packet_ticks = GetTimerSystemTime() - packet_started;
                native_presented = display.present_model2(machine.video().background_layer(),
                    machine.video().foreground_layer(), machine.video().width(),
                    packets.vertices(), packets.batches(), machine.video().gpu_mem());
                if (!native_presented) {
                    if (!std::strcmp(display.native_stats().rejection, "NONE"))
                        throw std::runtime_error(display.error());
                    std::printf("GS FALLBACK frame=%u reason=%s\n", frame, display.native_stats().rejection);
                    const uint64_t fallback_started = GetTimerSystemTime();
                    machine.video().render_compact_reference();
                    fallback_ticks = GetTimerSystemTime() - fallback_started;
                    const auto &fallback_profile = machine.video().last_profile();
                    video_profile.tile_cache += fallback_profile.tile_cache;
                    video_profile.tile_draw += fallback_profile.tile_draw;
                    video_profile.raster += fallback_profile.raster;
                    video_profile.composite += fallback_profile.composite;
                }
            }
            const auto &pixels = machine.video().screen();
            if (!game_visible)
                game_visible = std::any_of(pixels.begin(), pixels.end(),
                    [](uint32_t pixel) { return (pixel & 0x00ffffffu) != 0; });
            if (frame == 1)
                std::printf("first pixels=%lu first=%08lx visible=%u\n",
                    static_cast<unsigned long>(pixels.size()),
                    static_cast<unsigned long>(pixels.empty() ? 0 : pixels.front()), unsigned(game_visible));
            if (native_presented) {
                game_visible = game_visible || !packets.vertices().empty();
            } else if (game_visible) {
                if (!display.present_rgba(pixels, machine.video().width()))
                    throw std::runtime_error(display.error());
            } else if (debug_visible || debug_toggled || frame == 1 || frame % 60 == 0) {
                // Keep actual boot progress visible while the guest produces
                // only black frames. Once game pixels appear, present them.
                char message[160];
                std::snprintf(message, sizeof(message),
                    "GAME BOOT: FRAME %u\nPC %08X\nCACHE MISSES %llu\nWAITING FOR FIRST NONBLACK GAME FRAME",
                    frame, unsigned(run.pc), static_cast<unsigned long long>(cache->misses()));
                ::ps2::show_status(display, message);
            }
            const uint64_t finished = GetTimerSystemTime();
            flow.frame = frame;
            flow.total = finished - frame_started;
            flow.machine = run_finished - run_started + fallback_ticks;
            flow.tiles = video_profile.tile_cache + video_profile.tile_draw;
            flow.raster = video_profile.raster;
            flow.composite = video_profile.composite;
            flow.packets = packet_ticks;
            flow.submit = finished - run_finished - packet_ticks - fallback_ticks;
            flow.cache_misses = cache->misses();
            flow.cache_delta = flow.cache_misses - previous_misses;
            previous_misses = flow.cache_misses;
            flow.renderer = native_presented ? ::ps2::FlowSample::Renderer::Native :
                launch.native_gs ? ::ps2::FlowSample::Renderer::Fallback : ::ps2::FlowSample::Renderer::Cpu;
            const auto &native_stats = display.native_stats();
            flow.triangles = native_presented ? native_stats.triangles : 0;
            flow.texture_bytes = native_presented ? native_stats.texture_bytes : 0;
            flow.palette_bytes = native_presented ? native_stats.palette_bytes : 0;
            flow.layer_bytes = native_presented ? native_stats.layer_bytes :
                game_visible ? uint64_t(::ps2::kTextureWidth) * ::ps2::kFrameHeight * 4 : 0;
            flow.texture_hits = native_presented ? native_stats.texture_hits : 0;
            diagram.update(flow);
            if (frame <= 3 || frame % 60 == 0 || frame == 84) {
                std::printf("PS2_FLOW_BEGIN\n%s\nPS2_FLOW_END\n", diagram.text());
                if (native_presented)
                    std::printf("GS TRAFFIC frame=%u triangles=%lu texture_uploads=%lu texture_hits=%lu texture_bytes=%llu palette_uploads=%lu palette_hits=%lu palette_bytes=%llu layer_bytes=%llu\n",
                        frame, static_cast<unsigned long>(native_stats.triangles),
                        static_cast<unsigned long>(native_stats.texture_uploads),
                        static_cast<unsigned long>(native_stats.texture_hits),
                        static_cast<unsigned long long>(native_stats.texture_bytes),
                        static_cast<unsigned long>(native_stats.palette_uploads),
                        static_cast<unsigned long>(native_stats.palette_hits),
                        static_cast<unsigned long long>(native_stats.palette_bytes),
                        static_cast<unsigned long long>(native_stats.layer_bytes));
                const auto &profile = machine.video().last_profile();
                std::printf("PROFILE frame=%u total=%llu pad=%llu machine=%llu present=%llu tile_cache=%llu tile_draw=%llu raster=%llu composite=%llu cache_misses=%llu\n",
                    frame, static_cast<unsigned long long>(finished - frame_started),
                    static_cast<unsigned long long>(run_started - frame_started),
                    static_cast<unsigned long long>(run_finished - run_started),
                    static_cast<unsigned long long>(finished - run_finished),
                    static_cast<unsigned long long>(profile.tile_cache),
                    static_cast<unsigned long long>(profile.tile_draw),
                    static_cast<unsigned long long>(profile.raster),
                    static_cast<unsigned long long>(profile.composite),
                    static_cast<unsigned long long>(cache->misses()));
            }
#endif
            // A compact differential checkpoint each frame; no game bytes.
#ifdef RALLY_PS2_EE
            if (frame > 3 && frame % 60 != 0) continue;
            if (launch.native_gs && native_presented) {
                // Compact background pixels are not a full reference image.
                // Do not publish them as a misleading differential hash.
                std::printf("GS FRAME %u - REFERENCE SCREEN HASH NOT CAPTURED\n", frame);
                continue;
            }
#endif
            const auto stats = machine.stats(true);
            uint64_t registers = 0xcbf29ce484222325ULL;
            for (uint32_t r : machine.cpu().m_r)
                for (unsigned b = 0; b < 4; ++b) {
                    registers ^= uint8_t(r >> (b * 8));
                    registers *= 0x100000001b3ULL;
                }
            std::printf("%u,%llu,%llu,%llu,%08x,%016llx,%016llx\n", frame,
                    static_cast<unsigned long long>(stats.instructions),
                    static_cast<unsigned long long>(stats.reference_cycles),
                    static_cast<unsigned long long>(stats.board_time_ns), unsigned(stats.pc),
                    static_cast<unsigned long long>(registers),
                    static_cast<unsigned long long>(stats.screen_hash));
        }
        std::fprintf(stderr, "paged cache misses: %llu\n", static_cast<unsigned long long>(cache->misses()));
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "PS2 core bring-up: %s\n", error.what());
#ifdef RALLY_PS2_EE
        display.set_debug_page(nullptr);
        // Machine/cache temporaries have unwound. Native GS status rendering
        // needs no diagnostic image allocation; retain the error until restart.
        if (display_ready && ::ps2::show_status(display, error.what()))
            while (true) SleepThread();
#endif
        return 1;
    }
}
