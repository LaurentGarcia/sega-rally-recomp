// PS2 presentation boundary. No SDK handles or game-derived data.
#pragma once

#include "app/gpu/model2_frame.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace ps2 {
constexpr int kFrameWidth = 496;
constexpr int kFrameHeight = 384;
constexpr int kTextureWidth = 512;
constexpr int kTextureHeight = 512;

// GS PSMCT32 uses R,G,B,A in increasing byte addresses. Runtime frames use
// 0xAARRGGBB. Presentation is opaque; GS full alpha is 0x80, not 0xff.
constexpr uint32_t gs_pixel(uint32_t rgb) {
    return 0x80000000u | ((rgb & 0xffu) << 16) | (rgb & 0xff00u) | ((rgb >> 16) & 0xffu);
}

// Validate capacities before writing anything. Source and destination must not
// overlap. Padding in every uploaded row is black, so filtering cannot expose
// stale heap contents. Strides are in uint32_t pixels.
bool convert_frame(std::span<const uint32_t> source, int width, int height, int source_stride,
                   std::span<uint32_t> destination, int destination_stride);

// The initial GS primitive path accepts only ordinary opaque solid triangles.
// Textures, checker patterns and shader edge proxies need separate validated
// implementations; rejecting them prevents a plausible but incorrect image.
bool flat_frame_supported(std::span<const app::Model2Frame::PolyVertex> vertices,
                          std::span<const app::Model2Frame::Batch> batches);
} // namespace ps2
