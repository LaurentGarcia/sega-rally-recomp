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
    std::puts("Video GPU scratch: lazy allocation, resize, release and renderer switches passed");
}
