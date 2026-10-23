#include "gs_display.h"
#include "diagnostic_font.h"
#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <kernel.h>
#include <packet.h>
#include <malloc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace ps2 {
struct GsDisplay::Impl {
    framebuffer_t frames[2]{};
    zbuffer_t depth{};
    texbuffer_t texture{};
    packet_t *packet = nullptr;
    uint32_t *staging = nullptr;
    int target = 1, height = 448;
    bool ready = false;
    const char *failure = "GS NOT INITIALIZED";

    void submit(qword_t *end) {
        dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, int(end - packet->data), 0, 0);
        dma_wait_fast();
    }
    bool background(std::span<const uint32_t> source, int stride) {
        if (!ready) return false;
        if (!convert_frame(source, kFrameWidth, kFrameHeight, stride,
                           {staging, size_t(kTextureWidth) * kFrameHeight}, kTextureWidth)) {
            failure = "INVALID FRAME SIZE";
            return false;
        }
        // A REF DMA tag points outside the command packet. Flush that pixel
        // range explicitly before the GIF DMA can read it.
        SyncDCache(staging, staging + size_t(kTextureWidth) * kFrameHeight);
        qword_t *q = draw_texture_transfer(packet->data, staging, kTextureWidth, kFrameHeight,
                                          GS_PSM_32, texture.address, texture.width);
        q = draw_texture_flush(q);
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, int(q - packet->data), 0, 0);
        dma_wait_fast();
        q = draw_framebuffer(packet->data, 0, &frames[target]);
        q = draw_scissor_area(q, 0, 0, 639, 0, height - 1);
        texrect_t rect{};
        rect.v0 = {0.f, 0.f, 0};
        rect.v1 = {640.f, float(height), 0};
        rect.t0.u = rect.t0.v = 0.f;
        rect.t1.u = float(kFrameWidth);
        rect.t1.v = float(kFrameHeight);
        rect.color.r = rect.color.g = rect.color.b = rect.color.a = 0x80;
        rect.color.q = 1.f;
        q = draw_rect_textured(q, 0, &rect);
        q = draw_finish(q);
        submit(q);
        draw_wait_finish();
        return true;
    }
    void flip() {
        graph_wait_vsync();
        graph_set_framebuffer_filtered(frames[target].address, 640, GS_PSM_32, 0, 0);
        target ^= 1;
    }
};

GsDisplay::GsDisplay() : impl_(new (std::nothrow) Impl) {}
GsDisplay::~GsDisplay() {
    if (!impl_) return;
    if (impl_->packet) packet_free(impl_->packet);
    std::free(impl_->staging);
    delete impl_;
}
const char *GsDisplay::error() const { return impl_ ? impl_->failure : "DISPLAY ALLOCATION FAILED"; }
int GsDisplay::output_height() const { return impl_ ? impl_->height : 0; }

bool GsDisplay::initialize() {
    if (!impl_) return false;
    auto &d = *impl_;
    d.height = graph_get_region() == GRAPH_MODE_PAL ? 512 : 448;
    d.packet = packet_init(256, PACKET_NORMAL);
    d.staging = static_cast<uint32_t *>(memalign(128, size_t(kTextureWidth) * kFrameHeight * sizeof(uint32_t)));
    if (!d.packet || !d.staging) { d.failure = "GS BUFFER ALLOCATION FAILED"; return false; }
    if (dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0) < 0) { d.failure = "GIF DMA INIT FAILED"; return false; }
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    graph_vram_clear();
    for (auto &frame : d.frames) {
        frame.width = 640; frame.height = d.height; frame.psm = GS_PSM_32;
        const int address = graph_vram_allocate(frame.width, frame.height, frame.psm, GRAPH_ALIGN_PAGE);
        if (address < 0) { d.failure = "FRAMEBUFFER VRAM FULL"; return false; }
        frame.address = unsigned(address);
    }
    d.texture.width = kTextureWidth;
    d.texture.psm = GS_PSM_32;
    const int address = graph_vram_allocate(kTextureWidth, kTextureHeight, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    if (address < 0) { d.failure = "TEXTURE VRAM FULL"; return false; }
    d.texture.address = unsigned(address);
    d.texture.info = {9, 9, TEXTURE_COMPONENTS_RGB, TEXTURE_FUNCTION_DECAL};
    d.depth.enable = DRAW_DISABLE;
    d.depth.mask = 1;
    d.depth.method = ZTEST_METHOD_ALLPASS;
    d.depth.zsm = GS_ZBUF_32;
    if (graph_initialize(d.frames[0].address, 640, d.height, GS_PSM_32, 0, 0) < 0) {
        d.failure = "VIDEO MODE FAILED";
        return false;
    }
    draw_disable_blending();
    lod_t lod{};
    lod.calculation = LOD_USE_K;
    lod.mag_filter = LOD_MAG_NEAREST;
    lod.min_filter = LOD_MIN_NEAREST;
    clutbuffer_t clut{};
    clut.storage_mode = CLUT_STORAGE_MODE1;
    clut.load_method = CLUT_NO_LOAD;
    texwrap_t wrap{};
    wrap.horizontal = wrap.vertical = WRAP_REGION_CLAMP;
    wrap.maxu = kFrameWidth - 1; wrap.maxv = kFrameHeight - 1;
    for (auto &frame : d.frames) {
        qword_t *q = draw_setup_environment(d.packet->data, 0, &frame, &d.depth);
        q = draw_texture_sampling(q, 0, &lod);
        q = draw_texturebuffer(q, 0, &d.texture, &clut);
        q = draw_texture_wrapping(q, 0, &wrap);
        q = draw_clear(q, 0, 0.f, 0.f, 640.f, float(d.height), 0, 0, 0);
        q = draw_finish(q);
        d.submit(q);
        draw_wait_finish();
    }
    d.ready = true;
    d.failure = "NONE";
    std::printf("PS2 GS: 640x%d, double PSMCT32 buffers, 512x512 texture, GIF DMA\n", d.height);
    return true;
}

bool GsDisplay::present_rgba(std::span<const uint32_t> source, int stride) {
    if (!impl_ || !impl_->background(source, stride)) return false;
    impl_->flip();
    return true;
}

bool GsDisplay::present_status(const char *message) {
    if (!impl_ || !impl_->ready) return false;
    auto &d = *impl_;
    qword_t *q = draw_framebuffer(d.packet->data, 0, &d.frames[d.target]);
    q = draw_scissor_area(q, 0, 0, 639, 0, d.height - 1);
    q = draw_clear(q, 0, 0.f, 0.f, 640.f, float(d.height), 16, 25, 35);
    const float sx = 640.f / kFrameWidth, sy = float(d.height) / kFrameHeight;
    const auto letter = [&](int x, int y, char c, uint32_t color, int scale) {
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        const auto rows = diagnostic::glyph(c);
        // Merge adjacent lit pixels into one sprite per horizontal run.
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5;) {
                if (!(rows[size_t(row)] & (1u << (4 - col)))) { ++col; continue; }
                const int first = col++;
                while (col < 5 && (rows[size_t(row)] & (1u << (4 - col)))) ++col;
                rect_t rect{};
                rect.v0 = {float(x + first * scale) * sx, float(y + row * scale) * sy, 0};
                rect.v1 = {float(x + col * scale) * sx, float(y + (row + 1) * scale) * sy, 0};
                rect.color.r = uint8_t(color >> 16);
                rect.color.g = uint8_t(color >> 8);
                rect.color.b = uint8_t(color);
                rect.color.a = 0x80;
                rect.color.q = 1.f;
                if (q - d.packet->data > 220) { d.submit(q); q = d.packet->data; }
                q = draw_rect_filled(q, 0, &rect);
            }
        }
    };
    int x = 24;
    for (const char *title = "SEGA RALLY - PS2 PORT"; *title; ++title, x += 12)
        letter(x, 28, *title, 0xff963du, 2);
    x = 24;
    int y = 72;
    for (const char *value = message ? message : "NO STATUS"; *value && y < 340; ++value) {
        if (*value == '\n') { x = 24; y += 14; continue; }
        if (x > 464) { x = 24; y += 14; }
        if (y >= 340) break;
        letter(x, y, *value, 0xeeeeeeu, 1);
        x += 6;
    }
    q = draw_finish(q);
    d.submit(q);
    draw_wait_finish();
    d.flip();
    d.failure = "NONE";
    return true;
}

bool GsDisplay::present_flat(std::span<const uint32_t> background, int stride,
                            std::span<const app::Model2Frame::PolyVertex> vertices,
                            std::span<const app::Model2Frame::Batch> batches) {
    if (!impl_) return false;
    if (!flat_frame_supported(vertices, batches)) { impl_->failure = "UNSUPPORTED MODEL2 PACKET"; return false; }
    auto &d = *impl_;
    if (!d.background(background, stride)) return false;
    const float sx = 640.f / kFrameWidth, sy = float(d.height) / kFrameHeight;
    // Model2Frame stores first-owner polygons first. For opaque triangles the
    // inverse submission order preserves that priority without a GS Z buffer.
    // This deliberately does not claim the shader's subpixel edge coverage.
    for (size_t bi = batches.size(); bi > 0; --bi) {
        const auto &batch = batches[bi - 1];
        qword_t *q = draw_scissor_area(d.packet->data, 0,
            int(std::ceil(batch.clip.x * sx)), int(std::ceil((batch.clip.x + batch.clip.w) * sx)) - 1,
            int(std::ceil(batch.clip.y * sy)), int(std::ceil((batch.clip.y + batch.clip.h) * sy)) - 1);
        for (size_t i = size_t(batch.first) + batch.count; i > batch.first; i -= 3) {
            const auto &a = vertices[i - 3], &b = vertices[i - 2], &c = vertices[i - 1];
            triangle_t triangle{};
            triangle.v0 = {a.x * sx, a.y * sy, 0};
            triangle.v1 = {b.x * sx, b.y * sy, 0};
            triangle.v2 = {c.x * sx, c.y * sy, 0};
            triangle.color.r = uint8_t(std::lround(a.r * 255.f));
            triangle.color.g = uint8_t(std::lround(a.g * 255.f));
            triangle.color.b = uint8_t(std::lround(a.b * 255.f));
            triangle.color.a = 0x80; triangle.color.q = 1.f;
            q = draw_triangle_filled(q, 0, &triangle);
            if (q - d.packet->data > 220) { d.submit(q); q = d.packet->data; }
        }
        q = draw_finish(q);
        d.submit(q);
        draw_wait_finish();
    }
    d.flip();
    return true;
}
} // namespace ps2
