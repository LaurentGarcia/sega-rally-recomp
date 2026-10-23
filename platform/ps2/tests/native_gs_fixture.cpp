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
#ifdef RALLY_PS2_TEST_OVERLAY
        display.set_debug_page(
            "ROM-FREE DEBUG PAGE FIXTURE\n"
            "+--------+      +--------+\n"
            "| EE CPU | ---> | VU0 XY |\n"
            "+--------+      +--------+\n"
            "                    |\n"
            "                    V\n"
            "                +---------+\n"
            "                | GS / GIF|\n"
            "                +---------+\n"
            "CHECKER + CUTOUT + STQ + TILES\n"
            "GLYPHS: [] <> == 100% UNDER_SCORE\n"
            "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
            "(OPTIONAL) * LOWERCASE LETTERING\n"
            "PIXEL UPLOAD BUFFER: 64 KIB");
#endif
        for (unsigned frame = 0; frame < 4; ++frame) {
            if (frame == 2) ++scene.memory.tex_generation;
            if (!display.present_model2(scene.background, scene.foreground, ps2::kFrameWidth,
                                        scene.vertices, scene.batches, scene.memory))
                throw std::runtime_error(display.error());
            const auto &stats = display.native_stats();
            std::printf("NATIVE GS frame=%u triangles=%lu checker=%lu uploads=%lu hits=%lu bytes=%llu palettes=%lu palette_hits=%lu palette_bytes=%llu layers=%llu\n",
                frame, static_cast<unsigned long>(stats.triangles), static_cast<unsigned long>(stats.checker_triangles),
                static_cast<unsigned long>(stats.texture_uploads), static_cast<unsigned long>(stats.texture_hits),
                static_cast<unsigned long long>(stats.texture_bytes), static_cast<unsigned long>(stats.palette_uploads),
                static_cast<unsigned long>(stats.palette_hits), static_cast<unsigned long long>(stats.palette_bytes),
                static_cast<unsigned long long>(stats.layer_bytes));
            if (stats.triangles != 3 || stats.checker_triangles != 1 || stats.texture_uploads != (frame % 2 ? 0 : 1) ||
                stats.texture_hits != (frame % 2 ? 2 : 1) || stats.palette_uploads != (frame ? 0 : 2) ||
                stats.palette_hits != (frame ? 2 : 0)) throw std::runtime_error("texture/CLUT cache counters differ");
        }
        std::puts("NATIVE GS submission/cache PASS; compare captured checker, cutout, STQ and tile overlay separately");
        while (true) SleepThread();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "NATIVE GS FAIL: %s\n", error.what());
        while (true) SleepThread();
    }
}
