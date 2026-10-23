// External-renderer scratch must not occupy CPU-only host memory. Switching
// back to an external renderer must recreate every buffer before its writes.
#include "runtime/raster.h"
#include "runtime/video_profile.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
#define private public
#include "runtime/video.h"
#undef private

static size_t gpu_capacity(const rt::Video &video) {
    return (video.background_gpu_.capacity() + video.foreground_gpu_.capacity() +
            video.gpu_pens_.capacity() + video.gpu_front_source_.capacity()) * sizeof(uint32_t) +
            video.gpu_tile_words_.capacity() * sizeof(uint16_t);
}

int main() {
    std::vector<uint8_t> tiles(0x10000), chars(0x80000), palette(0x4000), translation(0xc000), luma(0x20000);
    std::vector<uint32_t> texture(0x80000);
    const rt::VideoMem memory{palette.data(), translation.data(), luma.data(), texture.data(), texture.data(), 0};
    rt::Video video(tiles.data(), chars.data());
    const std::vector<rt::GeoPoly> polygons;
    assert(gpu_capacity(video) == 0);
#ifdef M2_COMPACT_TILEMAP
    assert(video.dec_chars_.capacity() == 0 && video.dec_char_dirty_.capacity() == 0);
    assert(video.dec_tiles_.capacity() == 0 && video.system24_tile_generations_.capacity() == 0);
    for (int layer = 0; layer < 4; ++layer)
        assert(video.pixmap_[layer].capacity() == 0 && video.flags_[layer].capacity() == 0);
#endif
    video.frame_start(); video.screen_update(polygons, 0, memory);
    const auto cpu_hash = video.screen_hash();
    assert(gpu_capacity(video) == 0);
    video.set_wide_margin(40);
    assert(gpu_capacity(video) == 0);
    video.set_external_3d(true, true);
#ifdef M2_COMPACT_TILEMAP
    assert(video.dec_chars_.size() == 0x80000 && video.dec_char_dirty_.size() == 0x4000);
    assert(video.dec_tiles_.size() == 4 * 4096 && video.system24_tile_generations_.size() == 4 * 4096);
    for (int layer = 0; layer < 4; ++layer)
        assert(video.pixmap_[layer].size() == 512 * 512 && video.flags_[layer].size() == 512 * 512);
#endif
    assert(video.background_layer().size() == size_t(576) * 384);
    assert(video.foreground_layer().size() == size_t(576) * 384);
    assert(video.gpu_tile_words_.size() == rt::Video::kGpuTileWords);
    assert(video.gpu_pens_.size() == rt::Video::kGpuPens);
    video.frame_start(); video.screen_update(polygons, 0, memory);
    const auto old_generation = video.background_generation();
    video.set_external_3d(false);
    assert(gpu_capacity(video) == 0 && video.background_generation() > old_generation);
#ifdef M2_COMPACT_TILEMAP
    assert(video.dec_chars_.capacity() == 0 && video.dec_char_dirty_.capacity() == 0);
    assert(video.dec_tiles_.capacity() == 0 && video.system24_tile_generations_.capacity() == 0);
    for (int layer = 0; layer < 4; ++layer)
        assert(video.pixmap_[layer].capacity() == 0 && video.flags_[layer].capacity() == 0);
#endif
    video.set_wide_margin(0);
    video.frame_start(); video.screen_update(polygons, 0, memory);
    assert(video.screen_hash() == cpu_hash && gpu_capacity(video) == 0);
    video.set_external_3d(true); // non-desktop external path
    video.frame_start(); video.screen_update(polygons, 0, memory);
    assert(video.background_layer().size() == size_t(496) * 384);
    assert(video.foreground_layer().size() == size_t(496) * 384);
    assert(gpu_capacity(video) == 2 * size_t(496) * 384 * 4 + 0x3000 * 2 + 4096 * 4);
    video.set_external_3d(true, true);
    video.set_wide_margin(24);
    video.frame_start(); video.screen_update(polygons, 0, memory);
#ifdef M2_VITA_RENDER_OPT
    // Vita uploads a native-width backdrop and expands it in its GPU pass.
    assert(video.background_layer().size() == size_t(496) * 384);
#else
    assert(video.background_layer().size() == size_t(544) * 384);
#endif
    video.set_external_3d(false);
    assert(gpu_capacity(video) == 0);
#ifdef M2_COMPACT_TILEMAP
    video.set_compact_external_3d(true);
    video.set_wide_margin(40);
    assert(video.width() == rt::Video::W);
    video.frame_start(); video.screen_update(polygons, 0, memory);
    assert(gpu_capacity(video) == 0 && video.dec_chars_.capacity() == 0);
    assert(video.background_layer().data() == video.screen().data());
    assert(video.foreground_layer().size() >= video.screen().size());
    for (int layer = 0; layer < 4; ++layer)
        assert(video.pixmap_[layer].capacity() == 0 && video.flags_[layer].capacity() == 0);
    video.render_compact_reference();
    assert(video.screen_hash() == cpu_hash && video.compact_external_3d());
    assert(gpu_capacity(video) == 0);

    // A rejected native frame must restore visible polygon pixels, not just
    // reproduce an empty layer. Use an original flat triangle over black.
    video.set_compact_external_3d(false);
    video.xhout_w(uint16_t(-84)); video.xvout_w(uint16_t(-130));
    video.palette_dirty_ = false;
    video.pens_[0] = 0xff000000;
    std::fill(translation.begin(), translation.end(), 255);
    rt::GeoPoly triangle;
    triangle.num_vertices = 3; triangle.z = 100; triangle.luma = 252;
    triangle.center[1] = 384;
    triangle.viewport[2] = 495; triangle.viewport[3] = 384;
    triangle.v[0] = {16.f, -16.f, {1.f, 0.f, 0.f}};
    triangle.v[1] = {160.f, -16.f, {1.f, 0.f, 0.f}};
    triangle.v[2] = {16.f, -160.f, {1.f, 0.f, 0.f}};
    const std::vector<rt::GeoPoly> visible{triangle};
    video.frame_start(); video.screen_update(visible, 0, memory);
    const auto reference = video.screen();
    assert(std::any_of(reference.begin(), reference.end(), [](uint32_t p) { return p != 0xff000000; }));
    video.set_compact_external_3d(true);
    video.frame_start(); video.screen_update(visible, 0, memory);
    assert(video.background_layer() != reference && gpu_capacity(video) == 0);
    video.render_compact_reference();
    assert(video.screen() == reference && video.compact_external_3d());
    video.render_compact_reference(); // A second request must not blend again.
    assert(video.screen() == reference);
    // Palette/translation writes can occur without a new geometrizer frame.
    // The CPU path retains the already drawn 3D layer in that case; fallback
    // must not regenerate it against the changed live memory (frame 325).
    std::fill(translation.begin(), translation.end(), 0);
    video.screen_update(visible, 0, memory);
    video.render_compact_reference();
    assert(video.screen() == reference);
    std::fill(translation.begin(), translation.end(), 255);
    video.frame_start(); video.screen_update(visible, 0, memory);
    assert(video.background_layer() != reference && gpu_capacity(video) == 0);
    video.set_external_3d(true, true);
    assert(!video.compact_external_3d() && gpu_capacity(video) != 0);
    video.set_external_3d(false);
    assert(gpu_capacity(video) == 0);
#endif
    std::puts("Video GPU scratch: lazy allocation, resize, release and renderer switches passed");
}
