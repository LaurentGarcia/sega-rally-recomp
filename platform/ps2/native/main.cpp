// Independent PS2-native rewrite bootstrap. Original diagnostic art/tone only;
// the recovered race systems are not yet connected into a playable game.
#include "native_display.h"
#include "pad_input.h"
#include "audio_output.h"
#include "debug_controls.h"
#include <kernel.h>
#include <timer.h>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    ps2::native::Display display;
    char page[1600]{}; // Outlives the renderer's retained diagnostic pointer.
    try {
        if (!display.initialize()) throw std::runtime_error(display.error());
        display.set_vu0_packing(true); // Validated final XYZ conversion only.
        display.present_status("NATIVE PS2 REWRITE\nPLATFORM BOOTSTRAP\nNO ARCADE PROCESSOR CORES\nINITIALIZING INPUT AND AUDIO");
        ps2::PadInput pad;
        // No game filesystem is opened by this ROM-free target. It owns this
        // IOP startup; a future native asset loader must establish ownership.
        if (!pad.initialize(true)) std::printf("PAD: %s\n", pad.status());
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
        bool diagnostics = true;
        uint32_t tone_phase = 0;
        std::array<float, 4096> pcm{};
        ps2::AudioOutput::Stats audio_stats{};
        uint64_t previous_ticks = 0;
        std::puts("NATIVE PORT: direct EE program, native vertices/textures, VU0 XYZ, GIF/GS, IOP/SPU2");
        std::puts("NATIVE PORT: no arcade processor, bus, board scheduler or sound synthesizer linked");
        std::puts("NATIVE PORT: original diagnostic content; gameplay systems are not connected");
        for (unsigned frame = 1;; ++frame) {
            const auto started = GetTimerSystemTime();
            auto input = pad.poll();
            if (debug_toggle.update(input)) diagnostics = !diagnostics;
            const float offset = input.connected ? (float(input.steer) - 128.f) * 0.375f : 0.f;
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
            std::snprintf(page, sizeof(page),
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
                "UNDERRUN SILENCE %lu FR\n"
                "\n"
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
                static_cast<unsigned long>(audio_stats.silent_frames), frame,
                static_cast<unsigned long long>(previous_ticks * 1000 / kBUSCLK),
                static_cast<unsigned long>(gs.triangles),
                static_cast<unsigned long long>(gs.texture_bytes),
                static_cast<unsigned long long>(gs.palette_bytes));
            display.set_debug_page(diagnostics ? page : nullptr);
            if (!display.present({triangles, 0xff101923u})) throw std::runtime_error(display.error());
            previous_ticks = GetTimerSystemTime() - started;
            if (frame <= 3 || frame % 60 == 0)
                std::printf("NATIVE_FRAME frame=%u ticks=%llu frequency=%u triangles=%lu audio=%u\n%s\n",
                    frame, static_cast<unsigned long long>(previous_ticks), unsigned(kBUSCLK),
                    static_cast<unsigned long>(display.stats().triangles), unsigned(audio_ready), page);
        }
    } catch (const std::exception &failure) {
        display.set_debug_page(nullptr);
        std::printf("NATIVE PORT ERROR: %s\n", failure.what());
        if (display.present_status(failure.what())) while (true) SleepThread();
        return 1;
    }
}
