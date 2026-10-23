// Original ROM-free scene: verify GS state survives diagnostics, CPU reference
// presentation and an unsupported-frame fallback, without losing cached CLUTs.
#include "native_gs_scene.h"
#include "gs_display.h"
#include <kernel.h>
#include <cstdio>
#include <stdexcept>

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    try {
        ps2::test::NativeScene scene;
        ps2::GsDisplay display;
        if (!display.initialize()) throw std::runtime_error(display.error());
        const auto native = [&] {
            if (!display.present_model2(scene.background, scene.foreground, ps2::kFrameWidth,
                                        scene.vertices, scene.batches, scene.memory))
                throw std::runtime_error(display.error());
        };
        const auto cached = [&] {
            native();
            const auto &stats = display.native_stats();
            if (stats.texture_uploads || stats.palette_uploads || stats.texture_hits != 2 || stats.palette_hits != 2)
                throw std::runtime_error("texture or CLUT cache changed across presentation transition");
        };
        native();
        if (display.native_stats().texture_uploads != 1 || display.native_stats().palette_uploads != 2)
            throw std::runtime_error("cold cache counters differ");
        for (unsigned round = 0; round < 3; ++round) {
            display.set_debug_page("ORIGINAL GS TRANSITION FIXTURE\nNATIVE + DATE + PSMT4 + CLUT\nCPU REFERENCE / STATUS / RETURN");
            cached();
            if (!display.present_rgba(scene.background, ps2::kFrameWidth))
                throw std::runtime_error(display.error());
            cached();
            display.set_debug_page(nullptr);
            if (!display.present_rgba(scene.background, ps2::kFrameWidth) ||
                !display.present_status("ORIGINAL STATUS AFTER REFERENCE PRESENTATION"))
                throw std::runtime_error(display.error());
            cached();

            auto unsupported = scene.vertices;
            for (unsigned i = 6; i < 9; ++i) unsupported[i].info[0] |= 1u << 7;
            if (display.present_model2(scene.background, scene.foreground, ps2::kFrameWidth,
                                       unsupported, scene.batches, scene.memory))
                throw std::runtime_error("microtexture packet unexpectedly accepted");
            if (!display.present_rgba(scene.background, ps2::kFrameWidth))
                throw std::runtime_error(display.error());
            cached();
            std::printf("GS TRANSITIONS round=%u PASS; native caches reused after page/reference/status/fallback\n", round);
        }
        // Repeat the final image so a host emulator's deferred framebuffer
        // presentation cannot leave the immediately preceding reference visible.
        cached();
        std::puts("GS TRANSITIONS PASS; compare final native checker/cutout/STQ image with the original fixture");
        while (true) SleepThread();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "GS TRANSITIONS FAIL: %s\n", error.what());
        while (true) SleepThread();
    }
}
