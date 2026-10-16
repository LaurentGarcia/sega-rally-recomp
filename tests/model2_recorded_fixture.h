// Reader for private historical renderer snapshots; no captured data in source.
#pragma once
#include "runtime/video.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace model2_recorded_fixture {
inline void require(bool ok, const std::string &why) {
    if (!ok) throw std::runtime_error("recorded Model2 scene: " + why);
}
template<class T = uint8_t> std::vector<T> read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "cannot open " + path.string());
    const auto length = file.tellg();
    require(length >= 0 && size_t(length) % sizeof(T) == 0 && size_t(length) <= 64 * 1024 * 1024, "invalid region length");
    std::vector<T> result(size_t(length) / sizeof(T));
    file.seekg(0); file.read(reinterpret_cast<char *>(result.data()), length);
    require(bool(file), "cannot read " + path.string()); return result;
}
struct Scene {
    std::filesystem::path path;
    unsigned frame = 0;
    int windows = 0, cx = 0, cy = 0, rx = 0, ry = 0;
    uint64_t generation = 0;
    std::vector<rt::GeoPoly> polygons;
    std::vector<uint8_t> palette, translation, luma, tiles, characters;
    std::vector<uint32_t> texture0, texture1;
    explicit Scene(std::filesystem::path directory) : path(std::move(directory)) {
        static_assert(sizeof(rt::GeoPoly) == 200);
        size_t polygon_size = 0;
        std::ifstream metadata(path / "scene.meta");
        metadata >> frame >> windows >> cx >> cy >> rx >> ry >> generation >> polygon_size;
        require(bool(metadata) && polygon_size == sizeof(rt::GeoPoly), "metadata/GeoPoly layout mismatch");
        polygons = read<rt::GeoPoly>(path / "geopolys.bin");
        for (const auto &p : polygons) require(p.num_vertices <= 8, "invalid vertex count");
        palette = read(path / "palram.bin"); translation = read(path / "colorxlat.bin");
        luma = read(path / "lumaram.bin"); tiles = read(path / "tile-ram.bin"); characters = read(path / "character-ram.bin");
        texture0 = read<uint32_t>(path / "tex0.bin"); texture1 = read<uint32_t>(path / "tex1.bin");
        require(palette.size() == 0x4000 && translation.size() == 0xc000 && luma.size() == 0x20000 &&
            tiles.size() == 0x10000 && characters.size() == 0x80000 && texture0.size() == 0x80000 &&
            texture1.size() == 0x80000, "region sizes");
    }
    // Scene must outlive the returned Video: Video keeps polygon/memory views.
    std::unique_ptr<rt::Video> video(bool external) const {
        require(cx == rx && cy == ry, "render offsets differ");
        auto result = std::make_unique<rt::Video>(tiles.data(), characters.data());
        result->xhout_w(uint16_t(cx - 84)); result->xvout_w(uint16_t(cy - 130));
        for (unsigned i = 0; i < 0x1000; ++i) result->palette_w(i, palette.data(), translation.data());
        result->set_external_3d(external, true); result->frame_start();
        result->screen_update(polygons, windows, {palette.data(), translation.data(), luma.data(), texture0.data(), texture1.data(), generation});
        return result;
    }
    void check_composition(const rt::Video &video) const {
        auto equal = [&](const std::string &name, const void *bytes, size_t size) {
            const auto saved = read(path / name);
            require(saved.size() == size && std::memcmp(saved.data(), bytes, size) == 0, "composition " + name);
        };
        equal("gpu-tile-words.bin", video.gpu_tile_words(), rt::Video::kGpuTileWords * 2);
        equal("gpu-pens.bin", video.gpu_pens(), rt::Video::kGpuPens * 4);
        for (int layer = 0; layer < 4; ++layer) {
            equal("tile-pixels-" + std::to_string(layer) + ".bin", video.system24_pixels(layer), 512 * 512 * 2);
            equal("tile-flags-" + std::to_string(layer) + ".bin", video.system24_flags(layer), 512 * 512);
        }
    }
};
} // namespace model2_recorded_fixture
