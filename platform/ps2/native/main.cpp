// Independent PS2-native bootstrap, static study, or native vehicle test.
// The vehicle test starts at one developer checkpoint; race flow is pending.
#include "native_display.h"
#include "pad_input.h"
#include "audio_output.h"
#include "debug_controls.h"
#include "native_scene.h"
#include "native_vehicle_demo.h"
#include "storage.h"
#include <kernel.h>
#include <dma_registers.h>
#include <sifrpc.h>
#include <timer.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    ps2::native::Display display;
    char page[1600]{}; // Outlives the renderer's retained diagnostic pointer.
    try {
        // This standalone application owns DMA startup. Enable the master
        // controller before any rendering, preserving all other control bits.
        // Channel initialization alone does not establish DMAE.
        const u32 dma_before=*DMA_REG_CTRL;
        *DMA_REG_CTRL=dma_before|1u;
        asm volatile("sync":::"memory");
        if(*DMA_REG_CTRL!=(dma_before|1u))throw std::runtime_error("NATIVE DMA ENABLE FAILED");
        std::printf("NATIVE_DMA_START before=%08lx after=%08lx\n",(unsigned long)dma_before,(unsigned long)*DMA_REG_CTRL);
        if (!display.initialize()) throw std::runtime_error(display.error());
        display.set_vu0_packing(true); // Validated final XYZ conversion only.
        display.present_status("NATIVE PS2 REWRITE\nPLATFORM BOOTSTRAP\nNO ARCADE PROCESSOR CORES\nINITIALIZING INPUT AND AUDIO");
        // Preserve loader-provided host modules before considering IOP reset.
        SifInitRpc(0);
        const bool exact_drive = argc > 1 && std::strcmp(argv[1],"--drive-ee")==0;
        const bool guarded_drive = argc > 1 && std::strcmp(argv[1],"--drive-guarded")==0;
        const bool explicit_drive = exact_drive || guarded_drive || (argc > 1 && std::strcmp(argv[1],"--drive")==0);
        if (explicit_drive && argc != 5)
            throw std::runtime_error("USE --drive, --drive-ee OR --drive-guarded CHECKPOINT COURSE BODY");
        const char *checkpoint_path = explicit_drive ? argv[2] : nullptr;
        const char *course_path = explicit_drive ? argv[3] : "host:native-course.bin";
        const char *body_path = explicit_drive ? argv[4] : "host:native-body.bin";
        const char *scene_path = !explicit_drive && argc > 1 ? argv[1] : nullptr;
        if (argc <= 1) {
            if (FILE *probe = std::fopen("host:native-driving.bin", "rb")) {
                std::fclose(probe); checkpoint_path = "host:native-driving.bin";
            }
        }
        if (!scene_path && !checkpoint_path) {
            if (FILE *probe = std::fopen("host:native-scene.bin", "rb")) {
                std::fclose(probe); scene_path = "host:native-scene.bin";
            }
        }
        bool retain_host = false, use_storage = false;
        const auto inspect_path = [&](const char* path) {
            if (!path) return;
            if (std::strncmp(path,"host:",5)==0) retain_host=true;
            else if (std::strncmp(path,"mass:",5)==0) use_storage=true;
            else throw std::runtime_error("NATIVE ASSET PATH MUST USE HOST: OR MASS:");
        };
        inspect_path(scene_path);
        if (checkpoint_path) {inspect_path(checkpoint_path);inspect_path(course_path);inspect_path(body_path);}
        ps2::PadInput pad;
        if (!pad.initialize(!retain_host)) std::printf("PAD: %s\n", pad.status());
        if (use_storage && !ps2::initialize_storage()) throw std::runtime_error(ps2::storage_error());
        if (checkpoint_path) {
            if (!ps2::wait_for_file(checkpoint_path)||!ps2::wait_for_file(course_path)||!ps2::wait_for_file(body_path))
                throw std::runtime_error(ps2::storage_error());
            return ps2::native::run_native_vehicle_demo(display,pad,checkpoint_path,course_path,body_path,
                exact_drive?ps2::native::NativeVehicleRenderMode::reference:guarded_drive?
                ps2::native::NativeVehicleRenderMode::guarded_vu:ps2::native::NativeVehicleRenderMode::native_vu);
        }
        ps2::native::Mesh mesh;
        ps2::native::StudyScene study;
        ps2::native::StudyCamera camera;
        if (scene_path) {
            display.present_status("STATIC GEOMETRY STUDY\nLOADING OWNER PROVIDED NATIVE MESH\nDIAGNOSTIC CAMERA / FLAT MATERIALS\nNO GAMEPLAY OR GAME CAMERA CLAIM");
            if (!ps2::wait_for_file(scene_path)) throw std::runtime_error(ps2::storage_error());
            ps2::native::MeshError mesh_error;
            if (!ps2::native::load_mesh_file(scene_path,mesh,mesh_error))
                throw std::runtime_error(ps2::native::mesh_error_message(mesh_error));
            const auto begin = GetTimerSystemTime();
            ps2::native::StudyError scene_error;
            if (!study.prepare(mesh,camera,scene_error))
                throw std::runtime_error(ps2::native::study_error_message(scene_error));
            const auto &stats=study.stats();
            std::printf("STATIC_GEOMETRY_STUDY path=%s crc=%08lx vertices=%lu faces=%lu objects=%lu output=%lu persistent_bytes=%lu prepare_ticks=%llu\n",
                scene_path,static_cast<unsigned long>(mesh.payload_crc32),static_cast<unsigned long>(stats.source_vertices),
                static_cast<unsigned long>(stats.source_faces),static_cast<unsigned long>(stats.source_objects),
                static_cast<unsigned long>(stats.output_triangles),static_cast<unsigned long>(stats.persistent_bytes),
                static_cast<unsigned long long>(GetTimerSystemTime()-begin));
            display.set_caption("STATIC GEOMETRY STUDY\nDIAGNOSTIC CAMERA / FLAT SHADING");
        }
        ps2::AudioOutput audio;
        const bool audio_ready = audio.initialize();
        if (!audio_ready) std::printf("AUDIO UNAVAILABLE: %s\n", audio.error());

        std::array<uint8_t, 32 * 32 / 2> indices{};
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; x += 2) {
                const auto color = (y / 8 + x / 8) & 1u;
                indices[y * 16 + x / 2] = uint8_t(color | (color << 4));
            }
        const ps2::native::Texture texture{indices.data(), indices.size(), 32, 32, 1};
        ps2::native::Material material;
        material.texture = &texture;
        material.palette.fill(0xffeffaffu);
        material.palette[0] = 0xff16a6c5u;
        material.palette[1] = 0xffff9754u;
        std::array<ps2::native::Triangle, 2> triangles;
        for (auto &triangle : triangles) triangle.material = &material;
        ps2::DebugPageToggle debug_toggle;
        bool diagnostics = scene_path == nullptr;
        uint16_t previous_buttons = 0;
        uint32_t tone_phase = 0;
        std::array<float, 4096> pcm{};
        ps2::AudioOutput::Stats audio_stats{};
        uint64_t previous_ticks = 0;
        std::puts(scene_path ? "NATIVE STUDY: EE transform/clip, VU0 XY packing, solid GIF/GS Z16S; VU1 inactive" :
                              "NATIVE PORT: direct EE program, native vertices/textures, VU0 XYZ, GIF/GS, IOP/SPU2");
        std::puts("NATIVE PORT: no arcade processor, bus, board scheduler or sound synthesizer linked");
        std::puts(scene_path ? "NATIVE STUDY: owner-provided mesh; diagnostic camera/lighting, no connected gameplay" :
                              "NATIVE PORT: original diagnostic content; gameplay systems are not connected");
        for (unsigned frame = 1;; ++frame) {
            const auto started = GetTimerSystemTime();
            auto input = pad.poll();
            if (debug_toggle.update(input)) diagnostics = !diagnostics;
            const auto pressed = input.buttons & ~previous_buttons;
            previous_buttons = input.buttons;
            if (scene_path && (pressed & (ps2::Left|ps2::Right|ps2::L1|ps2::R1))) {
                if (pressed & ps2::Left) camera.yaw -= .15f;
                if (pressed & ps2::Right) camera.yaw += .15f;
                if (camera.yaw > 3.141593f) camera.yaw -= 6.2831853f;
                if (camera.yaw < -3.141593f) camera.yaw += 6.2831853f;
                if ((pressed & ps2::L1) && camera.distance > 1.75f) camera.distance -= .25f;
                if ((pressed & ps2::R1) && camera.distance < 11.75f) camera.distance += .25f;
                ps2::native::StudyError error;
                if (!study.prepare(mesh,camera,error)) throw std::runtime_error(ps2::native::study_error_message(error));
            }
            const float offset = input.connected ? (float(input.left_x) - 128.f) * 0.375f : 0.f;
            triangles[0].vertices = {{{100.f + offset, 100.f, 0.f, 0.f, 1.f},
                                      {396.f + offset, 100.f, 32.f, 0.f, 1.f},
                                      {100.f + offset, 284.f, 0.f, 32.f, 1.f}}};
            triangles[1].vertices = {{{396.f + offset, 100.f, 32.f, 0.f, 1.f},
                                      {396.f + offset, 284.f, 32.f, 32.f, 1.f},
                                      {100.f + offset, 284.f, 0.f, 32.f, 1.f}}};
            if (audio_ready) {
                // Original low-volume 440 Hz square wave, gated by Cross.
                // This probes native PCM delivery; it is not game synthesis.
                const bool tone = input.connected && (input.buttons & ps2::Cross);
                // Refill against the IOP device queue, independently of PAL/
                // NTSC display cadence. Its count is output frames at48kHz.
                audio_stats = audio.stats();
                if (*audio.error()) throw std::runtime_error(audio.error());
                constexpr unsigned target_frames = 1536;
                const auto missing = audio_stats.queued < target_frames ? target_frames - audio_stats.queued : 0;
                const unsigned input_frames = (missing * 44100u + 47999u) / 48000u;
                for (unsigned i = 0; i < input_frames; ++i) {
                    tone_phase = (tone_phase + 440) % 44100;
                    const float sample = tone ? (tone_phase < 22050 ? .05f : -.05f) : 0.f;
                    pcm[2 * i] = pcm[2 * i + 1] = sample;
                }
                if (!audio.submit(std::span<const float>(pcm.data(), input_frames * 2)) || !audio.flush())
                    throw std::runtime_error(audio.error());
                if (frame == 1 || frame % 60 == 0) {
                    audio_stats = audio.stats();
                    if (*audio.error()) throw std::runtime_error(audio.error());
                }
            }
            const auto &gs = display.stats(); // Last completed native frame.
            if (scene_path) {
                const auto &s=study.stats();
                std::snprintf(page,sizeof(page),
                    "STATIC GEOMETRY STUDY\n"
                    "OWNER PROVIDED MODEL SPACE MESH\n"
                    "CAPTURED OBJECT TO REFERENCE VIEW\n"
                    "DIAGNOSTIC FIT / ORBIT CAMERA\n"
                    "FLAT COLORS / STUDY LIGHTING\n"
                    "NATIVE CLIP / PROJECT / GS Z16S\n"
                    "NO RECOVERED GAME CAMERA CLAIM\n"
                    "NO GAMEPLAY OR DRIVING PHYSICS\n\n"
                    "VERTICES %lu / OBJECTS %lu\n"
                    "SOURCE FACES %lu / DRAW %lu\n"
                    "SCENE STORAGE %lu BYTES\n"
                    "MESH CRC %08lX / FRAME %u\n"
                    "VU0 XY PACKING / VU1 INACTIVE\n\n"
                    "SELECT+START TOGGLE THIS PAGE\n"
                    "LEFT/RIGHT STEP DIAGNOSTIC ORBIT\n"
                    "L1/R1 STEP DIAGNOSTIC DISTANCE\n"
                    "HOLD CROSS FOR QUIET TEST TONE\n"
                    "EMULATOR TIMING IS NOT CONSOLE SPEED",
                    static_cast<unsigned long>(s.source_vertices),static_cast<unsigned long>(s.source_objects),
                    static_cast<unsigned long>(s.source_faces),static_cast<unsigned long>(s.output_triangles),
                    static_cast<unsigned long>(s.persistent_bytes),static_cast<unsigned long>(mesh.payload_crc32),frame);
            } else std::snprintf(page, sizeof(page),
                "NATIVE PS2 REWRITE - PLATFORM CHECK\n"
                "GAME SYSTEMS NOT YET CONNECTED\n"
                "\n"
                "[DUALSHOCK] -> [NATIVE EE PROGRAM]\n"
                "                    |\n"
                "                 [VU0 XYZ]\n"
                "                    |\n"
                "                 [GIF] -> [GS] -> TV\n"
                "VU1 INACTIVE / GS BUSY UNMEASURED\n"
                "\n"
                "[NATIVE PCM] -> [IOP] -> [SPU2]\n"
                "AUDIO %s / QUEUE %lu FR\n"
                "RING EMPTY SILENCE %lu FR\n"
                "DMA LATE SERVICE %lu EVENTS\n"
                "FRAME %u / PREVIOUS %llu MS\n"
                "GS TRI %lu / TEX %llu B / CLUT %llu B\n"
                "NO ARCADE CPU OR DEVICE CORES\n"
                "ORIGINAL DIAGNOSTIC GEOMETRY ONLY\n"
                "\n"
                "SELECT+START TOGGLE DIAGNOSTICS\n"
                "STEER TO MOVE THE TEST GEOMETRY\n"
                "HOLD CROSS FOR A QUIET TEST TONE\n"
                "EMULATOR TIMING IS NOT CONSOLE SPEED",
                audio_ready ? "ON" : "UNAVAILABLE", static_cast<unsigned long>(audio_stats.queued),
                static_cast<unsigned long>(audio_stats.silent_frames),
                static_cast<unsigned long>(audio_stats.late_service_events), frame,
                static_cast<unsigned long long>(previous_ticks * 1000 / kBUSCLK),
                static_cast<unsigned long>(gs.triangles),
                static_cast<unsigned long long>(gs.texture_bytes),
                static_cast<unsigned long long>(gs.palette_bytes));
            display.set_debug_page(diagnostics ? page : nullptr);
            if (!display.present(scene_path ? study.frame() : ps2::native::Frame{triangles, 0xff101923u}))
                throw std::runtime_error(display.error());
            previous_ticks = GetTimerSystemTime() - started;
            if (frame <= 3 || frame % 60 == 0)
                std::printf("NATIVE_FRAME frame=%u ticks=%llu frequency=%u triangles=%lu audio=%u\n%s\n",
                    frame, static_cast<unsigned long long>(previous_ticks), unsigned(kBUSCLK),
                    static_cast<unsigned long>(display.stats().triangles), unsigned(audio_ready), page);
        }
    } catch (const std::exception &failure) {
        display.set_debug_page(nullptr);
        display.set_caption(nullptr);
        std::printf("NATIVE PORT ERROR: %s\n", failure.what());
        if (display.present_status(failure.what())) while (true) SleepThread();
        return 1;
    }
}
