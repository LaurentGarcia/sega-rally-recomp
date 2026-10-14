// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels, Ville Linde, Aaron Giles, Ben Templeman and contributors
//
// Shared Model 2 priority from this project's geo.cpp, raster.cpp and
// app/gpu/gpu_renderer.cpp. C-compatible so other lifted Model 2 games use
// the same quantization, order and GPU ownership rules. Z conversion is
// the Aaron Giles 1.8.23 -> 4.12 conversion retained from MAME model2_v.cpp.
#ifndef DAYTONA_MODEL2_PRIORITY_H
#define DAYTONA_MODEL2_PRIORITY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline uint16_t daytona_model2_z_bucket(float value, uint32_t z_adjust)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    int exponent = (int)((bits >> 23) & 0xffu) - (int)((z_adjust >> 23) & 0xffu);
    uint32_t mantissa = (bits & 0x7fffffu) + 0x400u;
    if (mantissa > 0x7fffffu) {
        ++exponent;
        mantissa = (mantissa & 0x7fffffu) >> 1;
    }
    mantissa >>= 11;
    if ((bits & 0x80000000u) || exponent < -12) return 0;
    if (exponent < 0) return (uint16_t)((mantissa | 0x1000u) >> -exponent);
    if (exponent < 15) return (uint16_t)(((exponent + 1) << 12) | mantissa);
    return 0xffffu;
}

// Geo::check_culling's face and link rules, before its Z clipping tests.
static inline int daytona_model2_polygon_visible(uint32_t attr, int front)
{
    if (((attr >> 17) & 1u) == 0 && !front) return 0;
    return ((attr >> 8) & 3u) != 0;
}

// Negative means a claims pixels before b: latest window, nearest bucket,
// newest submission. There is intentionally no raw camera-Z tie breaker.
static inline int daytona_model2_order_compare(uint8_t a_window, uint16_t a_z, size_t a_submission,
                                               uint8_t b_window, uint16_t b_z, size_t b_submission)
{
    if (a_window != b_window) return a_window > b_window ? -1 : 1;
    if (a_z != b_z) return a_z < b_z ? -1 : 1;
    if (a_submission != b_submission) return a_submission > b_submission ? -1 : 1;
    return 0;
}

// Zero-based visible draw rank. LESS with a cleared depth of 1 makes the
// first surviving fragment own the pixel, including holes from cutouts.
static inline float daytona_model2_rank_depth(size_t rank, size_t polygon_count)
{
    return (float)(rank + 1) * (1.0f / (float)(polygon_count + 2));
}

#endif
