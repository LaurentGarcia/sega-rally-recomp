// Renderer-only EE executable. All real graphics are loaded from the owner's
// private scene.bin; no game data or generated instructions are linked here.
#include "gs_display.h"
#include "gs_scene.h"
#include "status_screen.h"
#include <kernel.h>
#include <sifrpc.h>
#include <timer.h>
#include <malloc.h>
#include <cstdio>
#include <string>

namespace {
std::uint64_t hash_pixels(const std::vector<std::uint32_t> &pixels) {
    std::uint64_t hash = 14695981039346656037ull;
    for (auto pixel : pixels) for (unsigned byte = 0; byte < 4; ++byte)
        hash = (hash ^ std::uint8_t(pixel >> (byte * 8))) * 1099511628211ull;
    return hash;
}
}

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    SifInitRpc(0);
    ps2::GsDisplay display;
    if (!display.initialize()) { std::printf("GS REPLAY FAIL %s\n", display.error()); SleepThread(); return 1; }
    ps2::show_status(display, "PRIVATE GS SCENE REPLAY\nLOADING HOST:SCENE.BIN");
    ps2::GsScene scene;
    std::string error;
    const char *path = argc > 1 ? argv[1] : "host:scene.bin";
    if (!ps2::load_gs_scene(path, scene, error)) {
        std::printf("GS REPLAY FAIL %s\n", error.c_str());
        ps2::show_status(display, error.c_str()); SleepThread(); return 1;
    }
    const auto heap = mallinfo();
    const auto geometry_bytes = scene.vertices.capacity() * sizeof(scene.vertices[0]) +
                                scene.batches.capacity() * sizeof(scene.batches[0]);
    std::printf("GS REPLAY MEMORY vertex_capacity=%lu batch_capacity=%lu geometry_bytes=%lu allocated=%lu arena=%lu\n",
        (unsigned long)scene.vertices.capacity(), (unsigned long)scene.batches.capacity(),
        (unsigned long)geometry_bytes, (unsigned long)heap.uordblks, (unsigned long)heap.arena);
    // A tiny owner-created host file chooses one stable screenshot mode per
    // launch: R = exact CPU reference upload, S = GS scalar, V = GS VU0.
    // A mode remains visible until the owned emulator process is stopped.
    char mode = 'S';
    if (FILE *file = std::fopen("host:scene-mode.txt", "rb")) {
        const int value = std::fgetc(file);
        if (value == 'R' || value == 'S' || value == 'V') mode = char(value);
        std::fclose(file);
    }
    display.set_vu0_packing(mode == 'V');
    std::printf("GS REPLAY BEGIN frame=%lu vertices=%lu batches=%lu mode=%c reference_hash=%08lx%08lx\n",
        (unsigned long)scene.frame, (unsigned long)scene.vertices.size(), (unsigned long)scene.batches.size(), mode,
        (unsigned long)(hash_pixels(scene.reference) >> 32), (unsigned long)(hash_pixels(scene.reference) & 0xffffffffu));
    for (unsigned iteration = 0;; ++iteration) {
        const auto started = GetTimerSystemTime();
        const bool ok = mode == 'R' ? display.present_rgba(scene.reference, 496) :
            display.present_model2(scene.background, scene.foreground, 496,
                                   scene.vertices, scene.batches, scene.memory());
        const auto elapsed = GetTimerSystemTime() - started;
        if (!ok) {
            std::printf("GS REPLAY FAIL mode=%c error=%s\n", mode, display.error());
            ps2::show_status(display, display.error()); SleepThread(); return 1;
        }
        if (iteration < 2) {
            const auto &stats = display.native_stats();
            std::printf("GS REPLAY PRESENTED mode=%c iteration=%u ticks=%llu triangles=%lu textured=%lu checker=%lu uploads=%lu hits=%lu texture_bytes=%llu palette_uploads=%lu palette_hits=%lu palette_bytes=%llu layer_bytes=%llu\n",
                mode, iteration, (unsigned long long)elapsed,
                (unsigned long)stats.triangles, (unsigned long)stats.textured_triangles,
                (unsigned long)stats.checker_triangles, (unsigned long)stats.texture_uploads,
                (unsigned long)stats.texture_hits, (unsigned long long)stats.texture_bytes,
                (unsigned long)stats.palette_uploads, (unsigned long)stats.palette_hits,
                (unsigned long long)stats.palette_bytes,
                (unsigned long long)stats.layer_bytes);
        }
    }
}
