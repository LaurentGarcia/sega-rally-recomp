// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels, Ville Linde, Aaron Giles
// Polygon edge setup adapts runtime/raster.cpp's pinned MAME poly.h port.
#include "app/gpu/model2_frame.h"
#include "app/gpu/model2_resources.h"
#include "runtime/video.h"
#include "runtime/model2_priority.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

namespace app {
namespace {
inline uint16_t le16(const uint8_t *base, uint32_t index) { return uint16_t(base[index * 2] | base[index * 2 + 1] << 8); }
}
Model2Frame::Model2Frame() {
    const auto gamma = model2_resources::make_gamma();
    std::copy(gamma.begin(), gamma.end(), gamma_);
}

void Model2Frame::build(const rt::Video &video, int w, int h, int scale) {
    verts_.clear();
    batches_.clear();
    poly_edges_.clear();
    const std::vector<rt::GeoPoly> &polys = video.gpu_polys();
    const rt::VideoMem &mem = video.gpu_mem();
    const int windows = video.gpu_windows();
    // widescreen: the screen is `margin` wider on each side; x shifts by it
    const int margin = (w - rt::Video::W) / 2;
    const int crtc_x = video.crtc_x() + margin, crtc_y = video.crtc_y(), render_x = video.render_x() + margin,
              render_y = video.render_y();
    // HUD at the edges: the condition panel's own quads (its z, inside its box) move
    const int hud_dx = video.gpu_hud_shift();
    const float *hud_box = video.raster().hud_box();
    const uint16_t hud_z = video.raster().hud_z();
    if (polys.empty() || !mem.palram || !mem.colorxlat) return;
    // internal resolution: the mip level the rasterizer picks from z, less
    // log2(scale) (its levels are 128 units of fast_log2)
    const int lod_bias = int(std::lround(128.0 * std::log2(double(scale))));

    // Raster::render: windows from the last down to 0, low z first, newest first within z.
    order_.resize(polys.size());
    std::iota(order_.begin(), order_.end(), size_t(0));
    std::sort(order_.begin(), order_.end(), [&](size_t a, size_t b) {
        return daytona_model2_order_compare(polys[a].window, polys[a].z, a,
                                            polys[b].window, polys[b].z, b) < 0;
    });
    uint32_t drawn = 0;
    for (size_t i : order_) {
        const rt::GeoPoly &poly = polys[i];
        if (poly.window > windows || poly.num_vertices < 3) continue;
        const int renderer = (poly.texheader[0] >> 13) & 3;
        if (renderer == 1) continue; // translucent solid: the rasterizer draws nothing

        // HUD overlay quad? (Raster::render_one's test, on the projected outline)
        int dx = 0;
        if (hud_dx && poly.z == hud_z) {
            float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
            for (int k = 0; k < poly.num_vertices; k++) {
                const rt::GeoVertex &g0 = poly.v[k];
                const float z = g0.p[0] + std::numeric_limits<float>::min();
                const float px = float(crtc_x + poly.center[0]) + g0.x / z - float(margin);
                const float py = float((384 - poly.center[1]) + crtc_y) - g0.y / z;
                bx0 = std::min(bx0, px), bx1 = std::max(bx1, px), by0 = std::min(by0, py), by1 = std::max(by1, py);
            }
            constexpr float kTol = 1.5f;
            if (bx0 >= hud_box[0] - kTol && bx1 <= hud_box[1] + kTol && by0 >= hud_box[2] - kTol && by1 <= hud_box[3] + kTol)
                dx = hud_dx;
        }
        // clip rectangle (Raster::render_one), in the renderer's offsets, within the screen;
        // a viewport spanning the screen extends into the widescreen margins
        const int wide = margin && poly.viewport[0] <= 0 && poly.viewport[2] >= 495 ? margin : 0;
        const int x0 = std::max(poly.viewport[0] - wide + render_x + dx, 0),
                  x1 = std::min(poly.viewport[2] + wide + render_x + dx, w - 1);
        const int y0 = std::max((384 - poly.viewport[3]) + render_y, 0), y1 = std::min((384 - poly.viewport[1]) + render_y, h - 1);
        if (x0 > x1 || y0 > y1) continue;
        const Rect clip{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
        if (batches_.empty() || std::memcmp(&batches_.back().clip, &clip, sizeof clip) != 0)
            batches_.push_back({uint32_t(verts_.size()), 0, clip});

        // colour: the solid renderer's (palette entry, luma translation, gamma)
        const uint32_t colorbase = (poly.texheader[3] >> 6) & 0x3ff;
        const uint32_t luma = poly.luma >> 2;
        const uint32_t color = le16(mem.palram, colorbase + 0x1000);
        const float r = gamma_[le16(mem.colorxlat, 0x0000 / 2 + (((color >> 0) & 0x1f) << 8) + luma) & 0xff] / 255.0f;
        const float g = gamma_[le16(mem.colorxlat, 0x4000 / 2 + (((color >> 5) & 0x1f) << 8) + luma) & 0xff] / 255.0f;
        const float b = gamma_[le16(mem.colorxlat, 0x8000 / 2 + (((color >> 10) & 0x1f) << 8) + luma) & 0xff] / 255.0f;
        const float depth = daytona_model2_rank_depth(drawn++, order_.size());

        // texture state, decoded as render_one (m2.hlsl PolyIn)
        const uint32_t th0 = poly.texheader[0], th1 = poly.texheader[1], th2 = poly.texheader[2];
        const uint32_t mirx = (th0 >> 8) & 1, miry = (th0 >> 9) & 1;
        const uint32_t wlog = th0 & 7, hlog = (th0 >> 3) & 7;
        // the rasterizer's 30 - countl_zero(min(width, height)): log2(min) - 1, width = 32 << wlog
        const uint32_t max_level = 4 + std::min(wlog, hlog);
        uint32_t info[4], more[2];
        info[0] = uint32_t(renderer) | ((th0 >> 15) & 1) << 2 | (((th0 >> 6) & 1) & ~mirx) << 3 |
                  (((th0 >> 7) & 1) & ~miry) << 4 | mirx << 5 | miry << 6 | ((th0 >> 12) & 1) << 7 |
                  ((th0 >> 10) & 3) << 8 | ((th2 >> 12) & 1) << 10 | max_level << 12 | wlog << 17 | hlog << 20;
        info[1] = (32u * (th2 & 0x3f)) | (32u * ((th2 >> 6) & 0x1f)) << 16;
        info[2] = (((th2 >> 13) & 1) * 128) | (((th2 >> 14) & 3) * 128) << 16;
        info[3] = ((th1 & 0xff) << 7) | uint32_t(poly.luma) << 16;
        more[0] = uint32_t(poly.texlod + lod_bias);
        more[1] = le16(mem.palram, colorbase + 0x1000) & 0x7fff;

        // model2_3d_project. The default path uses a fan from vertex 0.
        PolyVertex v[8];
        for (int k = 0; k < poly.num_vertices; k++) {
            const rt::GeoVertex &g0 = poly.v[k];
            const float z = g0.p[0] + std::numeric_limits<float>::min();
            const float ooz = 1.0f / z; // render_one, textured: p[0] = 1/z, p[1..2] scaled by it / 8
            v[k] = {float(crtc_x + dx + poly.center[0]) + g0.x / z, float((384 - poly.center[1]) + crtc_y) - g0.y / z, depth,
                    ooz, g0.p[1] * ooz * (1.0f / 8.0f), g0.p[2] * ooz * (1.0f / 8.0f),
                    r, g, b, 1.0f, {info[0], info[1], info[2], info[3]}, {more[0], more[1]}, {0, 0}};
        }
        auto emit_proxy = [&](float min_y, float max_y, uint32_t first, uint32_t counts) {
            // Backend triangles cannot provide reference coverage at every
            // subpixel boundary. Draw a conservative rectangle, then reject pixels
            // outside the rounded CPU span in the common fragment shader.
            // One extra native pixel keeps rasterizer subpixel snapping
            // away from the actual boundary. The existing scissor still
            // owns window clipping; depth/rank stays one per polygon.
            float bx0 = v[0].x, bx1 = v[0].x;
            for (int k = 1; k < poly.num_vertices; ++k) {
                bx0 = std::min(bx0, v[k].x);
                bx1 = std::max(bx1, v[k].x);
            }
            bx0 = std::max(std::floor(bx0) - 1.0f, float(x0));
            bx1 = std::min(std::ceil(bx1) + 1.0f, float(x1 + 1));
            const float by0 = std::max(std::floor(min_y) - 1.0f, float(y0));
            const float by1 = std::min(std::ceil(max_y) + 1.0f, float(y1 + 1));
            if (bx0 < bx1 && by0 < by1) {
                PolyVertex rect[4] = {v[0], v[0], v[0], v[0]};
                rect[0].x = rect[2].x = bx0;
                rect[1].x = rect[3].x = bx1;
                rect[0].y = rect[1].y = by0;
                rect[2].y = rect[3].y = by1;
                for (auto &vertex : rect) vertex.edges[0] = first, vertex.edges[1] = counts;
                for (int k : {0, 1, 2, 1, 3, 2}) verts_.push_back(rect[k]);
            }
        };
        if (polygon_scanlines_ && poly.num_vertices == 3) {
            // Raster::render_triangle has a separate setup contract: stable
            // y sorting, direct slope divisions, and determinant planes.
            // It cannot use render_polygon's reciprocal-based edge chains.
            const PolyVertex *v1 = &v[0], *v2 = &v[1], *v3 = &v[2];
            if (v2->y < v1->y) std::swap(v1, v2);
            if (v3->y < v2->y) {
                std::swap(v2, v3);
                if (v2->y < v1->y) std::swap(v1, v2);
            }
            const float dxdy12 = v2->y == v1->y ? 0.0f : (v2->x - v1->x) / (v2->y - v1->y);
            const float dxdy13 = v3->y == v1->y ? 0.0f : (v3->x - v1->x) / (v3->y - v1->y);
            const float dxdy23 = v3->y == v2->y ? 0.0f : (v3->x - v2->x) / (v3->y - v2->y);
            const float a00 = v2->y - v3->y, a01 = v3->x - v2->x, a02 = v2->x * v3->y - v3->x * v2->y;
            const float a10 = v3->y - v1->y, a11 = v1->x - v3->x, a12 = v3->x * v1->y - v1->x * v3->y;
            const float a20 = v1->y - v2->y, a21 = v2->x - v1->x, a22 = v1->x * v2->y - v2->x * v1->y;
            const float det = a02 + a12 + a22;
            const float params[3][3] = {{v1->ooz, v1->uoz, v1->voz},
                                       {v2->ooz, v2->uoz, v2->voz},
                                       {v3->ooz, v3->uoz, v3->voz}};
            PolyEdge plane{}; // origin.xyz = start, step.xyz = dpdx, tail.xyz = dpdy
            plane.tail[3] = float(x0); // CPU plane evaluation starts at the clipped span
            if (std::abs(det) < 0.00001f) {
                for (int p = 0; p < 3; ++p) plane.origin[p] = params[0][p];
            } else {
                const float idet = 1.0f / det;
                for (int p = 0; p < 3; ++p) {
                    plane.step[p] = idet * (params[0][p] * a00 + params[1][p] * a10 + params[2][p] * a20);
                    plane.tail[p] = idet * (params[0][p] * a01 + params[1][p] * a11 + params[2][p] * a21);
                    plane.origin[p] = idet * (params[0][p] * a02 + params[1][p] * a12 + params[2][p] * a22);
                }
            }
            const uint32_t first = uint32_t(poly_edges_.size());
            poly_edges_.push_back({{v1->x, v1->y, v2->x, v2->y},
                                   {v3->x, v3->y, dxdy12, dxdy13}, {dxdy23, 0, 0, 0}});
            poly_edges_.push_back(plane);
            // Bit 7 is unused by polygon right-edge counts (at most seven).
            emit_proxy(v1->y, v3->y, first, 0x80u | uint32_t(scale) << 8);
        } else if (polygon_scanlines_ && poly.num_vertices >= 4) {
            // Raster::render_polygon's two chains from minimum to maximum y.
            // Upload geometry and parameter gradients; texture shading stays GPU.
            int minv = 0, maxv = 0;
            for (int k = 1; k < poly.num_vertices; ++k) {
                if (v[k].y < v[minv].y) minv = k;
                else if (v[k].y > v[maxv].y) maxv = k;
            }
            std::array<PolyEdge, 7> forward{}, backward{};
            uint32_t nf = 0, nb = 0;
            auto chain = [&](int direction, std::array<PolyEdge, 7> &edges, uint32_t &count) {
                for (int cur = minv; cur != maxv;) {
                    const int next = (cur + direction + poly.num_vertices) % poly.num_vertices;
                    const auto &a = v[cur], &b = v[next];
                    if (a.y != b.y) {
                        const float ooy = 1.0f / (b.y - a.y);
                        edges[count++] = {{a.x, a.y, a.ooz, a.uoz},
                            {(b.x - a.x) * ooy, (b.ooz - a.ooz) * ooy,
                             (b.uoz - a.uoz) * ooy, (b.voz - a.voz) * ooy},
                            {a.voz, b.y, v[maxv].y, float(cur)}};
                    }
                    cur = next;
                }
            };
            chain(1, forward, nf);
            chain(-1, backward, nb);
            if (nf && nb) {
                const auto &f = forward.front(), &b = backward.front();
                if (!((f.tail[3] == b.tail[3] && f.step[0] < b.step[0]) ||
                      (f.tail[3] != b.tail[3] && f.origin[0] < b.origin[0]))) {
                    forward.swap(backward);
                    std::swap(nf, nb);
                }
                const uint32_t first = uint32_t(poly_edges_.size());
                const uint32_t counts = nf | nb << 4 | uint32_t(scale) << 8;
                poly_edges_.insert(poly_edges_.end(), forward.begin(), forward.begin() + nf);
                poly_edges_.insert(poly_edges_.end(), backward.begin(), backward.begin() + nb);
                // The initial-vertex indices were needed only to order the
                // chains. Supply the global minimum y for the shader's row
                // bounds, even when a horizontal edge was skipped.
                for (size_t k = first; k < poly_edges_.size(); ++k) poly_edges_[k].tail[3] = v[minv].y;
                // The native span starts after clipping, before checker
                // alignment and repeated parameter stepping in the shader.
                poly_edges_.push_back({{float(x0), 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});

                emit_proxy(v[minv].y, v[maxv].y, first, counts);
            }
        } else {
            for (int k = 1; k + 1 < poly.num_vertices; k++) {
                verts_.push_back(v[0]);
                verts_.push_back(v[k]);
                verts_.push_back(v[k + 1]);
            }
        }
        batches_.back().count = uint32_t(verts_.size()) - batches_.back().first;
    }
}

} // namespace app
