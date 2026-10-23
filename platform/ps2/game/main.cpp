// Actual generated i960/TGP machine with paged images. The first EE target
// presents reference-rendered frames through GS DMA. Sound and native GS
// textured rendering are separate bring-up work, not silently approximated.
#include "platform/ps2/game/rom_cache.h"
#include "runtime/model2a_machine.h"
#include "runtime/video.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#ifdef RALLY_PS2_EE
#include "gs_display.h"
#include "pad_input.h"
#include "controls.h"
#include "storage.h"
#include "status_screen.h"
#include <kernel.h>
#include <malloc.h>
#include <sifrpc.h>
#else
#include <fstream>
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
#endif

#ifdef RALLY_PS2_EE
static void report_memory(const char *stage) {
    const auto info = mallinfo();
    std::printf("EE heap stage=%s arena=%lu allocated=%lu free_in_arena=%lu heap_end=%p\n",
        stage, static_cast<unsigned long>(info.arena), static_cast<unsigned long>(info.uordblks),
        static_cast<unsigned long>(info.fordblks), EndOfHeap());
}
#endif

static rt::Model2AMachine::RunResult run_frame(rt::Model2AMachine &machine, unsigned frame) {
    // Bounded slices retain instruction-boundary timing while making the first
    // native execution observable. The total per-frame guard is unchanged.
    constexpr unsigned total_budget = 50000000, slice_budget = 4096;
    rt::Model2AMachine::RunResult run;
    for (unsigned remaining = total_budget, slices = 0; remaining;) {
        const unsigned budget = std::min(remaining, slice_budget);
        run = machine.run_until_frame(budget);
        if (run.reason != rt::Model2AMachine::Reason::Budget) return run;
        remaining -= budget;
        ++slices;
#ifdef RALLY_PS2_EE
        if (frame == 1) {
            const auto stats = machine.stats();
            std::printf("boot slice=%u instructions=%llu cycles=%llu pc=%08x\n", slices,
                static_cast<unsigned long long>(stats.instructions),
                static_cast<unsigned long long>(stats.reference_cycles), unsigned(stats.pc));
        }
#else
        (void)frame;
#endif
    }
    return run;
}

int main(int argc, char **argv) {
#ifdef RALLY_PS2_EE
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    ::ps2::GsDisplay display;
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
        options.instruction_boundary_hook = true;
#ifdef RALLY_PS2_EE
        ::ps2::show_status(display, "PACK OPEN\nALLOCATING MODEL 2 MACHINE");
#endif
        rt::Model2AMachine machine(std::move(images), options);
#ifdef RALLY_PS2_EE
        report_memory("machine-ready");
        ::ps2::show_status(display, "MACHINE READY\nRUNNING TRANSLATED GAME PROGRAM");
        // First-frame stage markers only; returning zero deliberately records
        // no timing measurement. Remove the callback after the first frame.
        machine.video().set_profile_clock([]() -> uint64_t {
            static unsigned checkpoint = 0;
            std::printf("first video checkpoint=%u\n", ++checkpoint);
            return 0;
        });
        bool game_visible = false;
#endif
        std::puts("frame,instructions,cycles,board_ns,pc,register_hash,screen_hash");
        for (unsigned frame = 1; !frame_limit || frame <= frame_limit; ++frame) {
#ifdef RALLY_PS2_EE
            if (frame <= 60) std::printf("Polling controller for game frame %u\n", frame);
            machine.inputs() = controls.sample(pad.poll());
            if (frame <= 60)
                std::printf("Entering game frame %u at %08x\n", frame, unsigned(machine.cpu().m_IP));
#endif
            const auto run = run_frame(machine, frame);
#ifdef RALLY_PS2_EE
            if (frame <= 60)
                std::printf("game frame return=%u reason=%u pc=%08x\n",
                    frame, unsigned(run.reason), unsigned(run.pc));
            if (frame == 1) machine.video().set_profile_clock(nullptr);
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
#ifdef RALLY_PS2_EE
            const auto &pixels = machine.video().screen();
            if (!game_visible)
                game_visible = std::any_of(pixels.begin(), pixels.end(),
                    [](uint32_t pixel) { return (pixel & 0x00ffffffu) != 0; });
            if (frame == 1)
                std::printf("first pixels=%lu first=%08lx visible=%u\n",
                    static_cast<unsigned long>(pixels.size()),
                    static_cast<unsigned long>(pixels.empty() ? 0 : pixels.front()), unsigned(game_visible));
            if (game_visible) {
                if (!display.present_rgba(pixels, machine.video().width()))
                    throw std::runtime_error(display.error());
            } else if (frame == 1 || frame % 60 == 0) {
                // Keep actual boot progress visible while the guest produces
                // only black frames. Once game pixels appear, present them.
                char message[160];
                std::snprintf(message, sizeof(message),
                    "GAME BOOT: FRAME %u\nPC %08X\nCACHE MISSES %llu\nWAITING FOR FIRST NONBLACK GAME FRAME",
                    frame, unsigned(run.pc), static_cast<unsigned long long>(cache->misses()));
                ::ps2::show_status(display, message);
            }
#endif
            // A compact differential checkpoint each frame; no game bytes.
#ifdef RALLY_PS2_EE
            if (frame > 3 && frame % 60 != 0) continue;
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
        // Machine/cache temporaries have unwound. Native GS status rendering
        // needs no diagnostic image allocation; retain the error until restart.
        if (display_ready && ::ps2::show_status(display, error.what()))
            while (true) SleepThread();
#endif
        return 1;
    }
}
