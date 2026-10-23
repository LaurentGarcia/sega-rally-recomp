#include "native_display.h"
#include "diagnostic_font.h"
#include "vu0_pack.h"
#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <kernel.h>
#include <malloc.h>
#include <packet.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

namespace ps2::native {
namespace {
constexpr unsigned upload_bytes = 65536, texture_slots = 14, palette_slots = 256;
std::uint32_t gs_color(std::uint32_t argb) {
    return ((argb >> 16) & 255u) | (argb & 0xff00u) | ((argb & 255u) << 16) |
        (((argb >> 24) + 1u) / 2u << 24);
}
}


struct Display::Impl {
    framebuffer_t frames[2]{};
    zbuffer_t depth{};
    texbuffer_t font{};
    packet_t *packet = nullptr;
    std::uint32_t *staging = nullptr;
    unsigned target = 1, height = 448;
    bool ready = false, use_vu0 = false;
    const char *failure = "NATIVE GS NOT INITIALIZED", *debug = nullptr;
    Stats counters{};
    struct TextureSlot { texbuffer_t buffer{}; Texture key{}; std::uint64_t age = 0; bool valid = false; } textures[texture_slots];
    struct PaletteSlot {
        clutbuffer_t buffer{};
        std::array<std::uint32_t, 16> colors{};
        std::uint64_t age = 0;
        bool valid = false;
    } palettes[palette_slots];
    std::uint64_t texture_age = 0, palette_age = 0;

    void submit(qword_t *end) {
        dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, int(end - packet->data), 0, 0);
        dma_wait_fast();
    }
    void complete(qword_t *&q) {
        q = draw_finish(q);
        submit(q);
        draw_wait_finish();
        q = packet->data;
    }
    void upload(unsigned width, unsigned rows, unsigned psm, unsigned address, unsigned pitch, size_t bytes) {
        // SDK chain flushing covers descriptors only; REF pixels require their
        // own explicit data-cache synchronization before GIF reads them.
        SyncDCache(staging, reinterpret_cast<std::uint8_t *>(staging) + bytes);
        qword_t *q = draw_texture_transfer(packet->data, staging, width, rows, psm, address, pitch);
        q = draw_texture_flush(q);
        dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, int(q - packet->data), 0, 0);
        dma_wait_fast();
    }
    qword_t *test(qword_t *q, bool cutout) {
        atest_t alpha{DRAW_ENABLE, static_cast<unsigned char>(cutout ? ATEST_METHOD_GREATER_EQUAL : ATEST_METHOD_ALLPASS),
                      0x80, ATEST_KEEP_ALL};
        dtest_t destination{DRAW_DISABLE, DTEST_METHOD_PASS_ONE};
        ztest_t z{DRAW_ENABLE, ZTEST_METHOD_ALLPASS};
        return draw_pixel_test(q, 0, &alpha, &destination, &z);
    }
    qword_t *clear(std::uint32_t color) {
        qword_t *q = draw_framebuffer(packet->data, 0, &frames[target]);
        q = draw_scissor_area(q, 0, 0, 639, 0, int(height) - 1);
        q = test(q, false);
        return draw_clear(q, 0, 0, 0, 640, float(height), (color >> 16) & 255, (color >> 8) & 255, color & 255);
    }
    TextureSlot &texture(const Texture &key, qword_t *&q) {
        for (auto &slot : textures)
            if (slot.valid && slot.key.indices == key.indices && slot.key.generation == key.generation &&
                slot.key.width == key.width && slot.key.height == key.height) {
                slot.age = ++texture_age; ++counters.texture_hits; return slot;
            }
        auto *slot = &textures[0];
        for (auto &candidate : textures)
            if (!candidate.valid || (slot->valid && candidate.age < slot->age)) slot = &candidate;
        complete(q); // Earlier primitives must finish before VRAM is reused.
        const size_t bytes = size_t(key.width) * key.height / 2;
        std::memcpy(staging, key.indices, bytes);
        upload(key.width, key.height, GS_PSM_4, slot->buffer.address, slot->buffer.width, bytes);
        slot->buffer.info = {draw_log2(key.width), draw_log2(key.height), TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL};
        slot->key = key; slot->age = ++texture_age; slot->valid = true;
        ++counters.texture_uploads; counters.texture_bytes += bytes;
        return *slot;
    }
    PaletteSlot &palette(const std::array<std::uint32_t, 16> &colors, qword_t *&q) {
        for (auto &slot : palettes)
            if (slot.valid && slot.colors == colors) {
                slot.age = ++palette_age; ++counters.palette_hits; return slot;
            }
        auto *slot = &palettes[0];
        for (auto &candidate : palettes)
            if (!candidate.valid || (slot->valid && candidate.age < slot->age)) slot = &candidate;
        complete(q);
        for (unsigned index = 0; index < 16; ++index) staging[index] = gs_color(colors[index]);
        upload(8, 2, GS_PSM_32, slot->buffer.address, 64, 64);
        slot->colors = colors; slot->age = ++palette_age; slot->valid = true;
        ++counters.palette_uploads; counters.palette_bytes += 64;
        return *slot;
    }
    void page(const char *text) {
        if (!text || !*text) return;
        qword_t *q = clear(0xff0c1218u);
        clutbuffer_t clut{};
        clut.storage_mode = CLUT_STORAGE_MODE1; clut.load_method = CLUT_NO_LOAD;
        q = draw_texturebuffer(q, 0, &font, &clut);
        texwrap_t wrap{};
        wrap.horizontal = wrap.vertical = WRAP_REGION_CLAMP; wrap.maxu = 127; wrap.maxv = 63;
        q = draw_texture_wrapping(q, 0, &wrap);
        q = test(q, true);
        const float sx = 640.f / frame_width, sy = float(height) / frame_height;
        unsigned row = 0, column = 0;
        for (const char *p = text; *p && row < 23; ++p) {
            if (*p == '\n') { column = 0; ++row; continue; }
            if (column == 40) { column = 0; if (++row == 23) break; }
            unsigned c = static_cast<unsigned char>(*p);
            if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
            if (c > 32 && c < 128) {
                if (q - packet->data > 220) { submit(q); q = packet->data; }
                texrect_t glyph{};
                glyph.v0 = {float(8 + column * 12) * sx, float(8 + row * 16) * sy, 0};
                glyph.v1 = {float(18 + column * 12) * sx, float(22 + row * 16) * sy, 0};
                glyph.t0.u = float(((c - 32) % 16) * 8); glyph.t0.v = float(((c - 32) / 16) * 8);
                glyph.t1.u = glyph.t0.u + 5; glyph.t1.v = glyph.t0.v + 7;
                glyph.color.r = glyph.color.g = glyph.color.b = glyph.color.a = 0x80; glyph.color.q = 1;
                q = draw_rect_textured(q, 0, &glyph);
            }
            ++column;
        }
        complete(q);
    }
    void flip() {
        page(debug);
        graph_wait_vsync();
        graph_set_framebuffer_filtered(frames[target].address, 640, GS_PSM_32, 0, 0);
        target ^= 1;
    }
};

Display::Display() : impl_(new (std::nothrow) Impl) {}
Display::~Display() {
    if (!impl_) return;
    if (impl_->packet) packet_free(impl_->packet);
    std::free(impl_->staging);
    delete impl_;
}
const char *Display::error() const { return impl_ ? impl_->failure : "NATIVE DISPLAY ALLOCATION FAILED"; }
unsigned Display::output_height() const { return impl_ ? impl_->height : 0; }
const Stats &Display::stats() const { static const Stats empty; return impl_ ? impl_->counters : empty; }
void Display::set_vu0_packing(bool enabled) { if (impl_) impl_->use_vu0 = enabled && vu0_pack_available(); }
void Display::set_debug_page(const char *text) { if (impl_) impl_->debug = text; }

bool Display::initialize() {
    if (!impl_) return false;
    auto &d = *impl_;
    if (d.ready) return true;
    d.height = graph_get_region() == GRAPH_MODE_PAL ? 512 : 448;
    d.packet = packet_init(256, PACKET_NORMAL);
    d.staging = static_cast<std::uint32_t *>(memalign(128, upload_bytes));
    if (!d.packet || !d.staging) { d.failure = "NATIVE GS BUFFER ALLOCATION FAILED"; return false; }
    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    graph_vram_clear();
    for (auto &frame : d.frames) {
        const int address = graph_vram_allocate(640, d.height, GS_PSM_32, GRAPH_ALIGN_PAGE);
        if (address < 0) { d.failure = "NATIVE FRAMEBUFFER VRAM EXHAUSTED"; return false; }
        frame.address = address; frame.width = 640; frame.height = d.height; frame.psm = GS_PSM_32;
    }
    const int font = graph_vram_allocate(128, 64, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    if (font < 0) { d.failure = "NATIVE FONT VRAM EXHAUSTED"; return false; }
    d.font.address = font; d.font.width = 128; d.font.psm = GS_PSM_32;
    d.font.info = {7, 6, TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL};
    for (auto &slot : d.textures) {
        const int address = graph_vram_allocate(256, 256, GS_PSM_4, GRAPH_ALIGN_BLOCK);
        if (address < 0) { d.failure = "NATIVE TEXTURE VRAM EXHAUSTED"; return false; }
        slot.buffer.address = address; slot.buffer.width = 256; slot.buffer.psm = GS_PSM_4;
    }
    for (auto &slot : d.palettes) {
        const int address = graph_vram_allocate(8, 8, GS_PSM_32, GRAPH_ALIGN_BLOCK);
        if (address < 0) { d.failure = "NATIVE CLUT VRAM EXHAUSTED"; return false; }
        slot.buffer.address = address; slot.buffer.psm = GS_PSM_32;
        slot.buffer.storage_mode = CLUT_STORAGE_MODE1; slot.buffer.load_method = CLUT_LOAD;
    }
    d.depth.enable = DRAW_DISABLE; d.depth.mask = 1; d.depth.method = ZTEST_METHOD_ALLPASS; d.depth.zsm = GS_ZBUF_32;
    if (graph_initialize(d.frames[0].address, 640, d.height, GS_PSM_32, 0, 0) < 0) {
        d.failure = "NATIVE VIDEO MODE FAILED"; return false;
    }
    draw_disable_blending();
    lod_t lod{}; lod.calculation = LOD_USE_K; lod.mag_filter = LOD_MAG_NEAREST; lod.min_filter = LOD_MIN_NEAREST;
    for (auto &frame : d.frames) {
        qword_t *q = draw_setup_environment(d.packet->data, 0, &frame, &d.depth);
        q = draw_texture_sampling(q, 0, &lod);
        q = draw_clear(q, 0, 0, 0, 640, float(d.height), 0, 0, 0);
        d.complete(q);
    }
    std::fill_n(d.staging, 128 * 64, 0u);
    for (unsigned c = 32; c < 128; ++c) {
        const auto rows = diagnostic::glyph(char(c));
        const unsigned x = ((c - 32) % 16) * 8, y = ((c - 32) / 16) * 8;
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned column = 0; column < 5; ++column)
                if (rows[row] & (1u << (4 - column))) d.staging[(y + row) * 128 + x + column] = 0x80eeeeeeu;
    }
    d.upload(128, 64, GS_PSM_32, d.font.address, d.font.width, 128 * 64 * 4);
    d.ready = true; d.failure = "NONE";
    return true;
}

bool Display::present(const Frame &frame) {
    if (!impl_ || !impl_->ready) return false;
    auto &d = *impl_;
    if (!valid_frame(frame, d.failure)) return false;
    d.counters = {};
    qword_t *q = d.clear(frame.clear_color);
    const float sx = 640.f / frame_width, sy = float(d.height) / frame_height;
    for (const auto &triangle : frame.triangles) {
        if (q - d.packet->data > 200) { d.submit(q); q = d.packet->data; }
        const auto &material = *triangle.material;
        ++d.counters.triangles;
        q = d.test(q, material.cutout);
        if (!material.texture) {
            triangle_t solid{};
            const auto &v = triangle.vertices;
            solid.v0 = {v[0].x * sx, v[0].y * sy, 0}; solid.v1 = {v[1].x * sx, v[1].y * sy, 0};
            solid.v2 = {v[2].x * sx, v[2].y * sy, 0};
            solid.color.r = material.color >> 16; solid.color.g = material.color >> 8; solid.color.b = material.color;
            solid.color.a = ((material.color >> 24) + 1) / 2; solid.color.q = 1;
            q = draw_triangle_filled(q, 0, &solid);
            continue;
        }
        auto &texture = d.texture(*material.texture, q);
        auto &palette = d.palette(material.palette, q);
        q = draw_texturebuffer(q, 0, &texture.buffer, &palette.buffer);
        texwrap_t wrap{}; wrap.horizontal = wrap.vertical = WRAP_REPEAT;
        q = draw_texture_wrapping(q, 0, &wrap);
        prim_t primitive{}; primitive.type = PRIM_TRIANGLE; primitive.mapping = DRAW_ENABLE; primitive.mapping_type = PRIM_MAP_ST;
        color_t white{}; white.r = white.g = white.b = white.a = 0x80; white.q = 1;
        q = draw_prim_start(q, 0, &primitive, &white);
        const auto &v = triangle.vertices;
        GsCoordinates positions[2]{{{2048.f + v[0].x * sx, 2048.f + v[0].y * sy,
                                    2048.f + v[1].x * sx, 2048.f + v[1].y * sy}},
                                  {{2048.f + v[2].x * sx, 2048.f + v[2].y * sy, 0, 0}}};
        GsFixedCoordinates fixed[2];
        if (d.use_vu0) quantize_gs_xy_vu0(positions, fixed, 2);
        else quantize_gs_xy_scalar(positions, fixed, 2);
        std::uint64_t registers[10]{};
        for (unsigned k = 0; k < 3; ++k) {
            color_t color = white; color.q = v[k].q;
            texel_t st{}; st.s = v[k].u * v[k].q / material.texture->width; st.t = v[k].v * v[k].q / material.texture->height;
            registers[k * 3] = color.rgbaq; registers[k * 3 + 1] = st.uv;
            registers[k * 3 + 2] = std::uint64_t(fixed[k / 2].xy[(k % 2) * 2]) |
                std::uint64_t(fixed[k / 2].xy[(k % 2) * 2 + 1]) << 16;
        }
        std::memcpy(q, registers, sizeof registers); q += 5;
        q = draw_prim_end(q, 3, DRAW_STQ_REGLIST);
    }
    d.complete(q);
    d.flip(); d.failure = "NONE";
    return true;
}
bool Display::present_status(const char *message) {
    if (!impl_ || !impl_->ready) return false;
    impl_->page(message ? message : "NO STATUS");
    impl_->flip(); impl_->failure = "NONE";
    return true;
}
} // namespace ps2::native
