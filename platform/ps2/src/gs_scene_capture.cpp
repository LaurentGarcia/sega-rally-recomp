// Host-only diagnostic export. All captured bytes remain in ignored build/.
#include "gs_scene.h"
#include "runtime/video.h"
#include <algorithm>

#if defined(_EE) || !defined(M2_COMPACT_TILEMAP)
#error "GS scene capture requires the compact host diagnostic build"
#endif

namespace ps2 {
bool capture_gs_scene(const char *path, std::uint32_t frame, rt::Video &video,
                      std::string &error) {
    if (video.width() != rt::Video::W || video.external_3d()) {
        error = "GS capture requires native width and the CPU/compact layer mode";
        return false;
    }
    const auto memory = video.gpu_mem();
    if (!memory.palram || !memory.colorxlat || !memory.lumaram || !memory.tex0 || !memory.tex1) {
        error = "GS capture requires complete video memory";
        return false;
    }
    struct Restore {
        rt::Video &video;
        bool mode;
        ~Restore() { video.set_compact_external_3d(mode); }
    } restore{video, video.compact_external_3d()};
    try {
        video.set_compact_external_3d(true);
        video.screen_update(video.gpu_polys(), video.gpu_windows(), memory);
        app::Model2Frame packets;
        packets.set_polygon_scanlines(false);
        packets.build(video, rt::Video::W, rt::Video::H, 1);
        GsScene scene;
        scene.frame = frame;
        scene.texture_generation = memory.tex_generation;
        scene.vertices = packets.vertices(); scene.batches = packets.batches();
        scene.background = video.background_layer();
        scene.foreground.assign(video.foreground_layer().begin(), video.foreground_layer().begin() + rt::Video::W * rt::Video::H);
        video.render_compact_reference();
        scene.reference = video.screen();
        scene.palette.assign(memory.palram, memory.palram + 0x4000);
        scene.translation.assign(memory.colorxlat, memory.colorxlat + 0xc000);
        scene.luma.assign(memory.lumaram, memory.lumaram + 0x20000);
        scene.texture0.assign(memory.tex0, memory.tex0 + 0x80000);
        scene.texture1.assign(memory.tex1, memory.tex1 + 0x80000);
        return save_gs_scene(path, scene, error);
    } catch (const std::exception &failure) {
        error = failure.what();
        return false;
    }
}
} // namespace ps2
