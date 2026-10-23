#pragma once
#include "app/gpu/model2_frame.h"
#include "runtime/raster.h"
#include <string>
#include <vector>

namespace rt { class Video; }
namespace ps2 {
// Private renderer replay data. Captures contain game-derived graphics and
// belong only under ignored build/. The .bin suffix is source-export banned.
// SRPS2SC1 uses bounded little-endian binary32/u32 records and payload/header
// integrity hashes. It is diagnostic data, not a portable save-state format.
struct GsScene {
    std::uint32_t frame = 0;
    std::uint64_t texture_generation = 0;
    std::vector<app::Model2Frame::PolyVertex> vertices;
    std::vector<app::Model2Frame::Batch> batches;
    std::vector<std::uint8_t> palette, translation, luma;
    std::vector<std::uint32_t> texture0, texture1;
    std::vector<std::uint32_t> background, foreground, reference;
    rt::VideoMem memory() const;
};

// Reads at most 16 MiB, checking all lengths before allocating. Failure leaves
// destination unchanged. No ROM or scene data is embedded in the executable.
bool load_gs_scene(const char *path, GsScene &destination, std::string &error);

#ifndef _EE
// Writes atomically below this checkout's ignored build/ only. The caller must
// supply complete fixed-size memory sheets and native 496x384 tile layers.
bool save_gs_scene(const char *path, const GsScene &scene, std::string &error);
// Host-only capture helper for a native-width M2_COMPACT_TILEMAP Video. Builds
// projected packets with polygon_scanlines=false, snapshots the two compact
// layers, then restores exact CPU-reference pixels and the original compact
// mode. The following frame rebuilds its layers normally.
bool capture_gs_scene(const char *path, std::uint32_t frame, rt::Video &video,
                      std::string &error);
#endif
} // namespace ps2
