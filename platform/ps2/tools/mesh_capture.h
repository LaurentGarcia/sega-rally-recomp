#pragma once
// Host-only bridge. This header is forbidden from native game dependencies.
#include "runtime/geo.h"

namespace ps2::offline {
void begin_mesh_capture(const char *directory, unsigned frame);
void end_mesh_capture();
void capture_mesh_object(const rt::Geo::geo_state &state, rt::GeoPtr input,
                         std::uint32_t opcode, std::uint32_t texture_points,
                         std::uint32_t texture_headers, std::uint32_t object_address,
                         std::uint32_t declared_links);
void capture_direct_geometry();
} // namespace ps2::offline
