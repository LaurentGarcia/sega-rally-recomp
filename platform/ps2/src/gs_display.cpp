#include "gs_display.h"
#include "diagnostic_font.h"
#include "vu0_pack.h"
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
#include <cstring>
#include <new>
#include <limits>

namespace ps2 {
struct GsDisplay::Impl {
    framebuffer_t frames[2]{};
    zbuffer_t depth{};
    texbuffer_t texture{};
    texbuffer_t font{};
    packet_t *packet = nullptr;
    uint32_t *staging = nullptr;
    int target = 1, height = 448;
    bool ready = false;
    bool native_ready = false, use_vu0 = false;
    NativeGsStats stats{};
    struct TextureSlot {
        texbuffer_t texture{};
        NativeTexture key{};
        uint64_t age = 0;
        bool valid = false;
    } slots[kGsTextureSlots];
    struct PaletteSlot {
        clutbuffer_t clut{};
        std::array<uint32_t, 16> colors{};
        uint64_t age = 0;
        bool valid = false;
    } palettes[kGsPaletteSlots];
    uint64_t texture_age = 0, palette_age = 0;
    const char *debug_text = nullptr;
    const char *failure = "GS NOT INITIALIZED";

    void submit(qword_t *end) {
        dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, int(end - packet->data), 0, 0);
        dma_wait_fast();
    }
    qword_t *alpha_test(qword_t *q, bool cutout, bool checker = false) {
        atest_t alpha{DRAW_ENABLE, static_cast<unsigned char>(cutout ? ATEST_METHOD_GREATER_EQUAL : ATEST_METHOD_ALLPASS),
                      0x80, ATEST_KEEP_ALL};
        dtest_t destination{static_cast<unsigned char>(checker ? DRAW_ENABLE : DRAW_DISABLE), DTEST_METHOD_PASS_ONE};
        ztest_t z{DRAW_ENABLE, ZTEST_METHOD_ALLPASS};
        return draw_pixel_test(q, 0, &alpha, &destination, &z);
    }
    bool layer(std::span<const uint32_t> source, int stride, bool transparent, bool checker_mask = false) {
        if (!ready) return false;
        if (stride < kFrameWidth || size_t(stride) > (std::numeric_limits<size_t>::max() - kFrameWidth) / (kFrameHeight - 1) ||
            source.size() < size_t(stride) * (kFrameHeight - 1) + kFrameWidth) {
            failure = "INVALID FRAME SIZE";
            return false;
        }
        // PSMCT32 pages are64x32pixels. Starting every strip on a32-row
        // boundary lets its address advance by rows*bufferWidth without
        // changing the GS swizzled layout or the destination texture pitch.
        constexpr unsigned rows_per_upload = kGsUploadBytes / sizeof(uint32_t) / kTextureWidth;
        static_assert(rows_per_upload == 32);
        for (unsigned first = 0; first < kFrameHeight; first += rows_per_upload) {
            const unsigned rows = std::min(rows_per_upload, unsigned(kFrameHeight) - first);
            convert_frame(source.subspan(size_t(first) * stride), kFrameWidth, rows, stride,
                          {staging, kGsUploadBytes / sizeof(uint32_t)}, kTextureWidth);
            for (unsigned row = 0; row < rows; ++row) {
                const unsigned y = first + row;
                if (transparent) {
                    for (int x = 0; x < kFrameWidth; ++x)
                        if (!source[size_t(y) * stride + x]) staging[size_t(row) * kTextureWidth + x] = 0;
                    std::fill(staging + size_t(row) * kTextureWidth + kFrameWidth,
                              staging + size_t(row + 1) * kTextureWidth, 0u);
                }
                if (checker_mask)
                    for (int x = 0; x < kFrameWidth; ++x)
                        staging[size_t(row) * kTextureWidth + x] =
                            (staging[size_t(row) * kTextureWidth + x] & 0x00ffffffu) | (((x ^ y) & 1) ? 0x80000000u : 0);
            }
            // REF tags point outside the command packet; flush only the
            // current bounded strip before handing its memory to GIF DMA.
            SyncDCache(staging, staging + size_t(kTextureWidth) * rows);
            qword_t *upload = draw_texture_transfer(packet->data, staging, kTextureWidth, rows,
                GS_PSM_32, texture.address + first * texture.width, texture.width);
            upload = draw_texture_flush(upload);
            dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, int(upload - packet->data), 0, 0);
            dma_wait_fast();
        }
        qword_t *q = draw_framebuffer(packet->data, 0, &frames[target]);
        q = draw_scissor_area(q, 0, 0, 639, 0, height - 1);
        texture.info.components = transparent || checker_mask ? TEXTURE_COMPONENTS_RGBA : TEXTURE_COMPONENTS_RGB;
        clutbuffer_t clut{};
        clut.storage_mode = CLUT_STORAGE_MODE1;
        clut.load_method = CLUT_NO_LOAD;
        q = draw_texturebuffer(q, 0, &texture, &clut);
        texwrap_t wrap{};
        wrap.horizontal = wrap.vertical = WRAP_REGION_CLAMP;
        wrap.maxu = kFrameWidth - 1; wrap.maxv = kFrameHeight - 1;
        q = draw_texture_wrapping(q, 0, &wrap);
        q = alpha_test(q, transparent);
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
    bool background(std::span<const uint32_t> source, int stride) { return layer(source, stride, false); }
    TextureSlot &texture_slot(const NativeTexture &key, qword_t *&q) {
        for (auto &slot : slots) {
            if (slot.valid && slot.key.same_indices(key)) {
                slot.age = ++texture_age;
                ++stats.texture_hits;
                return slot;
            }
        }
        auto *slot = &slots[0];
        for (auto &candidate : slots)
            if (!candidate.valid || (slot->valid && candidate.age < slot->age)) slot = &candidate;
        // The packet and staging area are reused. FINISH ensures the GS has
        // consumed any old cached texture before its VRAM slot is overwritten.
        q = draw_finish(q);
        submit(q);
        draw_wait_finish();
        const unsigned width = key.upload_width(), height = key.upload_height();
        const size_t bytes = size_t(width) * height / 2;
        static_assert(kGsTextureSide * kGsTextureSide / 2 <= kGsUploadBytes);
        decode_native_indices(key, {reinterpret_cast<uint8_t *>(staging), kGsUploadBytes});
        SyncDCache(staging, reinterpret_cast<uint8_t *>(staging) + bytes);
        q = draw_texture_transfer(packet->data, staging, width, height, GS_PSM_4,
                                  slot->texture.address, slot->texture.width);
        q = draw_texture_flush(q);
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, int(q - packet->data), 0, 0);
        dma_wait_fast();
        q = packet->data;
        slot->texture.info = {draw_log2(width), draw_log2(height), TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL};
        slot->key = key;
        slot->valid = true;
        slot->age = ++texture_age;
        ++stats.texture_uploads;
        stats.texture_bytes += bytes;
        return *slot;
    }
    PaletteSlot &palette_slot(const std::array<uint32_t, 16> &colors, qword_t *&q) {
        for (auto &slot : palettes) {
            if (slot.valid && slot.colors == colors) {
                slot.age = ++palette_age;
                ++stats.palette_hits;
                return slot;
            }
        }
        auto *slot = &palettes[0];
        for (auto &candidate : palettes)
            if (!candidate.valid || (slot->valid && candidate.age < slot->age)) slot = &candidate;
        // A palette miss must preserve earlier triangles that still refer to
        // an evicted CLUT. Keep the same explicit completion boundary as an
        // index upload; cache hits only add a TEX0 load, with no DMA transfer.
        q = draw_finish(q);
        submit(q);
        draw_wait_finish();
        std::copy(colors.begin(), colors.end(), staging);
        SyncDCache(staging, staging + colors.size());
        q = draw_texture_transfer(packet->data, staging, 8, 2, GS_PSM_32, slot->clut.address, 64);
        q = draw_texture_flush(q);
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, int(q - packet->data), 0, 0);
        dma_wait_fast();
        q = packet->data;
        slot->colors = colors;
        slot->age = ++palette_age;
        slot->valid = true;
        ++stats.palette_uploads;
        stats.palette_bytes += colors.size() * sizeof(uint32_t);
        return *slot;
    }
    void debug_page() {
        if (!debug_text || !*debug_text) return;
        constexpr unsigned columns = 40, lines = 23;
        const float sx = 640.f / kFrameWidth, sy = float(height) / kFrameHeight;
        qword_t *q = draw_framebuffer(packet->data, 0, &frames[target]);
        q = draw_scissor_area(q, 0, 0, 639, 0, height - 1);
        q = alpha_test(q, false);
        rect_t panel{};
        panel.v0 = {0.f, 0.f, 0};
        panel.v1 = {640.f, float(height), 0};
        panel.color.r = 12; panel.color.g = 18; panel.color.b = 24;
        panel.color.a = 0x80; panel.color.q = 1.f;
        q = draw_rect_filled(q, 0, &panel);
        clutbuffer_t clut{};
        clut.storage_mode = CLUT_STORAGE_MODE1; clut.load_method = CLUT_NO_LOAD;
        q = draw_texturebuffer(q, 0, &font, &clut);
        texwrap_t wrap{};
        wrap.horizontal = wrap.vertical = WRAP_REGION_CLAMP;
        wrap.maxu = 127; wrap.maxv = 63;
        q = draw_texture_wrapping(q, 0, &wrap);
        q = alpha_test(q, true);
        unsigned row = 0, column = 0;
        for (const char *p = debug_text; *p && row < lines; ++p) {
            if (*p == '\n') { column = 0; ++row; continue; }
            if (column == columns) { column = 0; if (++row == lines) break; }
            const unsigned c = static_cast<unsigned char>(*p);
            if (c > 32 && c < 128) {
                if (q - packet->data > 220) { submit(q); q = packet->data; }
                texrect_t glyph{};
                glyph.v0 = {float(8 + column * 12) * sx, float(8 + row * 16) * sy, 0};
                glyph.v1 = {float(18 + column * 12) * sx, float(22 + row * 16) * sy, 0};
                glyph.t0.u = float(((c - 32) % 16) * 8); glyph.t0.v = float(((c - 32) / 16) * 8);
                glyph.t1.u = glyph.t0.u + 5; glyph.t1.v = glyph.t0.v + 7;
                glyph.color.r = glyph.color.g = glyph.color.b = glyph.color.a = 0x80; glyph.color.q = 1.f;
                q = draw_rect_textured(q, 0, &glyph);
            }
            ++column;
        }
        q = draw_finish(q);
        submit(q);
        draw_wait_finish();
    }
    void flip() {
        debug_page();
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
const NativeGsStats &GsDisplay::native_stats() const {
    static const NativeGsStats unavailable{.rejection = "DISPLAY ALLOCATION FAILED"};
    return impl_ ? impl_->stats : unavailable;
}
void GsDisplay::set_vu0_packing(bool enabled) { if (impl_) impl_->use_vu0 = enabled && vu0_pack_available(); }
void GsDisplay::set_debug_page(const char *text) { if (impl_) impl_->debug_text = text; }

bool GsDisplay::initialize() {
    if (!impl_) return false;
    auto &d = *impl_;
    d.height = graph_get_region() == GRAPH_MODE_PAL ? 512 : 448;
    d.packet = packet_init(256, PACKET_NORMAL);
    d.staging = static_cast<uint32_t *>(memalign(128, kGsUploadBytes));
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
    d.native_ready = true;
    for (auto &slot : d.slots) {
        const int cached = graph_vram_allocate(kGsTextureSide, kGsTextureSide, GS_PSM_4, GRAPH_ALIGN_BLOCK);
        if (cached < 0) { d.native_ready = false; break; }
        slot.texture.address = unsigned(cached);
        slot.texture.width = kGsTextureSide;
        slot.texture.psm = GS_PSM_4;
    }
    for (auto &slot : d.palettes) {
        // CSM1's first16colors occupy an8x2 PSMCT32 rectangle inside one
        // GS256-byte block. Reserve the complete block for each cached CLUT.
        const int cached = graph_vram_allocate(8, 8, GS_PSM_32, GRAPH_ALIGN_BLOCK);
        if (cached < 0) { d.native_ready = false; break; }
        slot.clut.address = unsigned(cached);
        slot.clut.psm = GS_PSM_32;
        slot.clut.storage_mode = CLUT_STORAGE_MODE1;
        slot.clut.load_method = CLUT_LOAD;
    }
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
    // The frame upload uses only the first384rows of its512x512 allocation.
    // Row384 starts a PSMCT32 page boundary, leaving this independent128x64
    // font atlas in otherwise unused VRAM. No additional VRAM or heap buffer.
    d.font.address = d.texture.address + kTextureWidth * kFrameHeight;
    d.font.width = 128; d.font.psm = GS_PSM_32;
    d.font.info = {7, 6, TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL};
    std::fill_n(d.staging, 128 * 64, 0u);
    for (unsigned c = 32; c < 128; ++c) {
        const char letter = char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        const auto rows = diagnostic::glyph(letter);
        const unsigned x = ((c - 32) % 16) * 8, y = ((c - 32) / 16) * 8;
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned column = 0; column < 5; ++column)
                if (rows[row] & (1u << (4 - column))) d.staging[(y + row) * 128 + x + column] = 0x80eeeeeeu;
    }
    SyncDCache(d.staging, d.staging + 128 * 64);
    qword_t *font_end = draw_texture_transfer(d.packet->data, d.staging, 128, 64, GS_PSM_32, d.font.address, d.font.width);
    font_end = draw_texture_flush(font_end);
    dma_channel_send_chain(DMA_CHANNEL_GIF, d.packet->data, int(font_end - d.packet->data), 0, 0);
    dma_wait_fast();
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
    q = d.alpha_test(q, false);
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

bool GsDisplay::present_model2(std::span<const uint32_t> background, std::span<const uint32_t> foreground,
                              int stride, std::span<const app::Model2Frame::PolyVertex> vertices,
                              std::span<const app::Model2Frame::Batch> batches, const rt::VideoMem &memory) {
    if (!impl_ || !impl_->ready) return false;
    auto &d = *impl_;
    if (!native_frame_supported(vertices, batches, memory, d.stats)) {
        d.failure = d.stats.rejection;
        return false;
    }
    if (!d.native_ready) { d.failure = d.stats.rejection = "NATIVE TEXTURE VRAM UNAVAILABLE"; return false; }
    const size_t required = stride >= kFrameWidth &&
        size_t(stride) <= (std::numeric_limits<size_t>::max() - kFrameWidth) / (kFrameHeight - 1) ?
        size_t(stride) * (kFrameHeight - 1) + kFrameWidth : 0;
    if (!required || background.size() < required || (!foreground.empty() && foreground.size() < required)) {
        d.failure = d.stats.rejection = "INVALID TILE LAYER SIZE";
        return false;
    }
    if (!d.layer(background, stride, false, true)) return false;
    d.stats.layer_bytes += size_t(kTextureWidth) * kFrameHeight * sizeof(uint32_t);
    const float sx = 640.f / kFrameWidth, sy = float(d.height) / kFrameHeight;
    for (size_t bi = batches.size(); bi > 0; --bi) {
        const auto &batch = batches[bi - 1];
        // Destination alpha holds the native checker parity. Preserve it
        // through all polygons, so DATE can gate checker primitives without
        // a CPU polygon layer or another mask upload.
        framebuffer_t masked = d.frames[d.target];
        masked.mask = 0xff000000u;
        qword_t *q = draw_framebuffer(d.packet->data, 0, &masked);
        q = draw_scissor_area(q, 0,
            int(std::ceil(batch.clip.x * sx)), int(std::ceil((batch.clip.x + batch.clip.w) * sx)) - 1,
            int(std::ceil(batch.clip.y * sy)), int(std::ceil((batch.clip.y + batch.clip.h) * sy)) - 1);
        for (size_t i = size_t(batch.first) + batch.count; i > batch.first; i -= 3) {
            const auto *v = &vertices[i - 3];
            if (q - d.packet->data > 200) { d.submit(q); q = d.packet->data; }
            if (!(v[0].info[0] & 2)) {
                q = d.alpha_test(q, false, v[0].info[0] & 4);
                triangle_t triangle{};
                triangle.v0 = {v[0].x * sx, v[0].y * sy, 0};
                triangle.v1 = {v[1].x * sx, v[1].y * sy, 0};
                triangle.v2 = {v[2].x * sx, v[2].y * sy, 0};
                triangle.color.r = uint8_t(std::lround(v[0].r * 255.f));
                triangle.color.g = uint8_t(std::lround(v[0].g * 255.f));
                triangle.color.b = uint8_t(std::lround(v[0].b * 255.f));
                triangle.color.a = 0x80; triangle.color.q = 1.f;
                q = draw_triangle_filled(q, 0, &triangle);
                continue;
            }
            NativeTexture key;
            native_texture(v[0], memory, key); // complete preflight already succeeded
            auto &slot = d.texture_slot(key, q);
            auto &palette = d.palette_slot(key.colors, q);
            q = draw_texturebuffer(q, 0, &slot.texture, &palette.clut);
            texwrap_t wrap{};
            // Repetition also implements the mirrored period baked into the
            // decoded texture. Clamp bits affect Model 2's filtered border,
            // which this explicit nearest-sampling path does not approximate.
            wrap.horizontal = wrap.vertical = WRAP_REPEAT;
            q = draw_texture_wrapping(q, 0, &wrap);
            q = d.alpha_test(q, (v[0].info[0] & 3) == 3, v[0].info[0] & 4);
            prim_t primitive{};
            primitive.type = PRIM_TRIANGLE;
            primitive.mapping = DRAW_ENABLE;
            primitive.mapping_type = PRIM_MAP_ST;
            color_t white{};
            white.r = white.g = white.b = white.a = 0x80; white.q = 1.f;
            q = draw_prim_start(q, 0, &primitive, &white);
            GsCoordinates positions[2]{{{2048.f + v[0].x * sx, 2048.f + v[0].y * sy,
                                        2048.f + v[1].x * sx, 2048.f + v[1].y * sy}},
                                      {{2048.f + v[2].x * sx, 2048.f + v[2].y * sy, 0.f, 0.f}}};
            GsFixedCoordinates fixed[2];
            if (d.use_vu0) quantize_gs_xy_vu0(positions, fixed, 2);
            else quantize_gs_xy_scalar(positions, fixed, 2);
            uint64_t registers[10]{};
            for (unsigned k = 0; k < 3; ++k) {
                color_t color = white; color.q = v[k].ooz;
                texel_t st{};
                st.s = v[k].uoz / float(key.upload_width());
                st.t = v[k].voz / float(key.upload_height());
                registers[k * 3] = color.rgbaq;
                registers[k * 3 + 1] = st.uv;
                registers[k * 3 + 2] = uint64_t(fixed[k / 2].xy[(k % 2) * 2]) |
                    uint64_t(fixed[k / 2].xy[(k % 2) * 2 + 1]) << 16;
            }
            std::memcpy(q, registers, sizeof registers);
            q += 5;
            q = draw_prim_end(q, 3, DRAW_STQ_REGLIST);
        }
        q = draw_finish(q);
        d.submit(q);
        draw_wait_finish();
    }
    if (!foreground.empty()) {
        if (!d.layer(foreground, stride, true)) return false;
        d.stats.layer_bytes += size_t(kTextureWidth) * kFrameHeight * sizeof(uint32_t);
    }
    d.flip();
    d.failure = "NONE";
    return true;
}
} // namespace ps2
