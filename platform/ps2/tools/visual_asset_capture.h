#pragma once
// Offline observation only; never a native asset/runtime dependency.
#if defined(_EE) || defined(RALLY_PS2_EE) || defined(M2_FAST_GEO)
#error "Visual asset capture requires the ordinary host reference geometry path"
#endif
#include "runtime/geo.h"
namespace rt { class Video; }
namespace ps2::offline {
void begin_visual_asset_frame(const char *directory,unsigned completed_frame);
void visual_asset_object(const rt::Geo::geo_state&,rt::GeoPtr,std::uint32_t opcode,
                         std::uint32_t points,std::uint32_t headers,std::uint32_t address,std::uint32_t links);
void visual_asset_polygon(const rt::Geo::raster_state&,std::uint32_t attributes,
                          const rt::GeoVertex*,const std::uint16_t*,unsigned luma,unsigned corners);
void visual_asset_object_end();
void end_visual_asset_frame(rt::Video&);
}
