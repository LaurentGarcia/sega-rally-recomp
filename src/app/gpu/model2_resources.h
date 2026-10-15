// Common shader resource packing. No GPU handles, backend state or SDL types.
#pragma once
#include "runtime/video.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace app::model2_resources {
inline constexpr uint32_t sheet_words=0x80000;
inline constexpr uint32_t texram_bytes=sheet_words*4*2;
inline constexpr uint32_t luma_bytes=0x20000;
inline constexpr uint32_t xlat_entries=0x6000;
inline constexpr uint32_t tilepix_bytes=4*512*512*2;
inline constexpr uint32_t tile_header=16;
inline constexpr uint32_t tiledata_words=tile_header+rt::Video::kGpuPens+rt::Video::kGpuTileWords/2;
inline std::array<uint8_t,256> make_gamma() {
    std::array<uint8_t,256> gamma{};
    for(unsigned i=0;i<256;++i)gamma[i]=uint8_t(std::max((double(i)-64.0)*255.0/191.0,0.0));
    return gamma;
}
// Caller provides xlat_entries uint32 entries; source is the little-endian
// hardware translation image. Preserve the original low-byte gamma lookup.
inline void pack_xlat(const uint8_t *source,const uint8_t *gamma,uint32_t *destination) {
    for(uint32_t i=0;i<xlat_entries;++i)destination[i]=gamma[uint16_t(source[2*i]|source[2*i+1]<<8)&0xff];
}
// Caller supplies layer0..3, first/count within512 rows and count*512 u16s.
inline void pack_tile_rows(const rt::Video &video,unsigned layer,unsigned first,unsigned count,uint16_t *destination) {
    const auto *pixels=video.system24_pixels(int(layer))+size_t(first)*512;
    const auto *flags=video.system24_flags(int(layer))+size_t(first)*512;
    for(uint32_t i=0;i<count*512;++i)destination[i]=uint16_t(pixels[i]|(flags[i]&1)<<15);
}
// Caller provides tiledata_words u32s; scale is the same1..4 internal scale
// used for Model2Frame. Zero reserved header words, then native packed images.
inline void pack_tile_data(const rt::Video &video,int scale,uint32_t *destination) {
    std::fill_n(destination,tiledata_words,0u);
    destination[0]=uint32_t(video.margin());destination[1]=uint32_t(video.backdrop());destination[2]=uint32_t(scale);
    std::memcpy(destination+tile_header,video.gpu_pens(),rt::Video::kGpuPens*4);
    std::memcpy(destination+tile_header+rt::Video::kGpuPens,video.gpu_tile_words(),rt::Video::kGpuTileWords*2);
}
} // namespace app::model2_resources
