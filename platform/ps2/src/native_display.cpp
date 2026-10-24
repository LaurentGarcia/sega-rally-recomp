#include "native_display.h"
#include "diagnostic_font.h"
#include "vu0_pack.h"
#include <dma.h>
#include <dma_registers.h>
#include <draw.h>
#include <ee_regs.h>
#include <gif_tags.h>
#include <graph.h>
#include <gs_gp.h>
#include <gs_privileged.h>
#include <gs_psm.h>
#include <kernel.h>
#include <malloc.h>
#include <packet.h>
#include <vif_registers.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace ps2::native {
static_assert(framebuffer_storage_format == GS_PSM_32);
static_assert(depth_storage_format == GS_PSMZ_16S && depth_register_format == GS_ZBUF_16S);
namespace {
constexpr unsigned upload_bytes = 65536, texture_slots = 14, palette_slots = 256;
constexpr std::uint32_t dma_running = 1u << 8, vif_reverse = 1u << 23;
// Display calls/lifetimes are caller-serialized. A failed transport poisons the
// global GS lease even after its C++ object dies: the reverse bus may be held.
const void *display_owner = nullptr;
bool display_quarantined = false;
// Fixed diagnostic bound in CP0 Count ticks; no claim about emulator wall time.
constexpr std::uint32_t readback_tick_limit = 300000000;
std::uint32_t count_ticks() {
    std::uint32_t count;
    asm volatile("mfc0 %0,$9" : "=r"(count));
    return count;
}
bool wait_bits(volatile u32 *reg, std::uint32_t mask, std::uint32_t expected) {
    const auto start = count_ticks();
    while ((*reg & mask) != expected)
        if (std::uint32_t(count_ticks() - start) >= readback_tick_limit) return false;
    return true;
}
bool wait_csr(u64 mask, u64 expected) {
    const auto start = count_ticks();
    while ((*GS_REG_CSR & mask) != expected)
        if (std::uint32_t(count_ticks() - start) >= readback_tick_limit) return false;
    return true;
}
bool stop_owned_dma(volatile u32 *chcr) {
    // EE User Manual v6 section5.7, forced stop: suspend globally only around
    // the channel-register edit; preserve other channels and interrupt state.
    const int interrupts = DIntr();
    const u32 hold = *DMA_REG_ENABLER;
    *DMA_REG_ENABLEW = hold | 0x10000u;
    const u32 control = *chcr; // Two uncached reads after suspension.
    const u32 dma_control = *DMA_REG_CTRL;
    (void)dma_control;
    *chcr = control & ~dma_running;
    asm volatile("sync.l" ::: "memory");
    const bool stopped = (*chcr & dma_running) == 0;
    *DMA_REG_ENABLEW = hold;
    if (interrupts) EIntr();
    return stopped;
}
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
    bool ready = false, use_vu0 = false, depth_active = false;
    bool transport_failed = false, retain_dma_memory = false;
    const char *failure = "NATIVE GS NOT INITIALIZED", *debug = nullptr, *caption = nullptr;
    Stats counters{};
    struct TextureSlot { texbuffer_t buffer{}; Texture key{}; std::uint64_t age = 0; bool valid = false; } textures[texture_slots];
    struct PaletteSlot {
        clutbuffer_t buffer{};
        std::array<std::uint32_t, 16> colors{};
        std::uint64_t age = 0;
        bool valid = false;
    } palettes[palette_slots];
    std::uint64_t texture_age = 0, palette_age = 0;

    void free_buffers() {
        if (packet) packet_free(packet);
        packet = nullptr;
        std::free(staging);
        staging = nullptr;
    }

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
    qword_t *test(qword_t *q, bool cutout, bool compare_depth = true) {
        atest_t alpha{DRAW_ENABLE, static_cast<unsigned char>(cutout ? ATEST_METHOD_GREATER_EQUAL : ATEST_METHOD_ALLPASS),
                      0x80, ATEST_KEEP_ALL};
        dtest_t destination{DRAW_DISABLE, DTEST_METHOD_PASS_ONE};
        ztest_t z{DRAW_ENABLE, static_cast<unsigned char>(depth_active && compare_depth ?
                     ZTEST_METHOD_GREATER_EQUAL : ZTEST_METHOD_ALLPASS)};
        return draw_pixel_test(q, 0, &alpha, &destination, &z);
    }
    qword_t *clear(std::uint32_t color, bool use_depth = false) {
        depth_active = use_depth;
        depth.mask = use_depth ? 0 : 1;
        qword_t *q = draw_framebuffer(packet->data, 0, &frames[target]);
        q = draw_zbuffer(q, 0, &depth);
        q = draw_scissor_area(q, 0, 0, 639, 0, int(height) - 1);
        // ALLPASS and unmasked Z writes clear native depth to0. Diagnostic
        // pages set use_depth=false, masking depth and disabling comparison.
        q = test(q, false, false);
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
    void page(const char *text, bool fullscreen = true) {
        if (!text || !*text) return;
        qword_t *q;
        if (fullscreen) q = clear(0xff0c1218u);
        else {
            depth_active = false; depth.mask = 1;
            q = draw_zbuffer(packet->data,0,&depth);
            q = test(q,false);
            rect_t panel{};
            panel.v0={0,0,0}; panel.v1={640,40.f*height/frame_height,0};
            panel.color.r=12; panel.color.g=18; panel.color.b=24; panel.color.a=0x80; panel.color.q=1;
            q=draw_rect_filled(q,0,&panel);
        }
        clutbuffer_t clut{};
        clut.storage_mode = CLUT_STORAGE_MODE1; clut.load_method = CLUT_NO_LOAD;
        q = draw_texturebuffer(q, 0, &font, &clut);
        texwrap_t wrap{};
        wrap.horizontal = wrap.vertical = WRAP_REGION_CLAMP; wrap.maxu = 127; wrap.maxv = 63;
        q = draw_texture_wrapping(q, 0, &wrap);
        q = test(q, true);
        const float sx = 640.f / frame_width, sy = float(height) / frame_height;
        unsigned row = 0, column = 0;
        const unsigned rows = fullscreen ? 23 : 2;
        for (const char *p = text; *p && row < rows; ++p) {
            if (*p == '\n') { column = 0; ++row; continue; }
            if (column == 40) { column = 0; if (++row == rows) break; }
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
    void flip(bool decorate = true) {
        if (decorate) {
            if (debug) page(debug);
            else page(caption,false);
        }
        graph_wait_vsync();
        graph_set_framebuffer_filtered(frames[target].address, 640, GS_PSM_32, 0, 0);
        target ^= 1;
    }
};

Display::Display() : impl_(new (std::nothrow) Impl) {}
Display::~Display() {
    if (!impl_) return;
    // A platform that cannot stop its owned DMA must not receive into freed
    // memory. Retain these bounded allocations on that fatal diagnostic path.
    if (!impl_->retain_dma_memory) impl_->free_buffers();
    if (display_owner == impl_) display_owner = nullptr;
    delete impl_;
}
const char *Display::error() const { return impl_ ? impl_->failure : "NATIVE DISPLAY ALLOCATION FAILED"; }
unsigned Display::output_height() const { return impl_ ? impl_->height : 0; }
const Stats &Display::stats() const { static const Stats empty; return impl_ ? impl_->counters : empty; }
void Display::set_vu0_packing(bool enabled) { if (impl_) impl_->use_vu0 = enabled && vu0_pack_available(); }
void Display::set_debug_page(const char *text) { if (impl_) impl_->debug = text; }
void Display::set_caption(const char *text) { if (impl_) impl_->caption = text; }
void Display::invalidate_textures() noexcept {
    if (!impl_) return;
    for (auto &slot : impl_->textures) {
        slot.valid = false;
        slot.key = {};
        slot.age = 0;
    }
    impl_->texture_age = 0;
}

bool Display::initialize() {
    if (!impl_) return false;
    auto &d = *impl_;
    if (d.transport_failed) return false;
    if (d.ready) return true;
    if (display_quarantined) {
        d.failure = "NATIVE GS TRANSPORT QUARANTINED; RESTART REQUIRED";
        return false;
    }
    if (display_owner) {
        d.failure = "NATIVE GS ALREADY OWNED BY ANOTHER DISPLAY";
        return false;
    }
    display_owner = impl_;
    const auto failed = [&d](const char *message) {
        d.free_buffers();
        display_owner = nullptr;
        d.failure = message;
        return false;
    };
    d.height = graph_get_region() == GRAPH_MODE_PAL ? 512 : 448;
    d.packet = packet_init(256, PACKET_NORMAL);
    d.staging = static_cast<std::uint32_t *>(memalign(128, upload_bytes));
    if (!d.packet || !d.staging) return failed("NATIVE GS BUFFER ALLOCATION FAILED");
    if (dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0) < 0)
        return failed("NATIVE GIF INITIALIZATION FAILED");
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    graph_vram_clear();
    for (auto &frame : d.frames) {
        const int address = graph_vram_allocate(640, d.height, GS_PSM_32, GRAPH_ALIGN_PAGE);
        if (address < 0) return failed("NATIVE FRAMEBUFFER VRAM EXHAUSTED");
        frame.address = address; frame.width = 640; frame.height = d.height; frame.psm = GS_PSM_32;
    }
    const int depth = graph_vram_allocate(640, d.height, depth_storage_format, GRAPH_ALIGN_PAGE);
    if (depth < 0) return failed("NATIVE DEPTH VRAM EXHAUSTED");
    d.depth.address = depth;
    const int font = graph_vram_allocate(128, 64, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    if (font < 0) return failed("NATIVE FONT VRAM EXHAUSTED");
    d.font.address = font; d.font.width = 128; d.font.psm = GS_PSM_32;
    d.font.info = {7, 6, TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL};
    for (auto &slot : d.textures) {
        const int address = graph_vram_allocate(256, 256, GS_PSM_4, GRAPH_ALIGN_BLOCK);
        if (address < 0) return failed("NATIVE TEXTURE VRAM EXHAUSTED");
        slot.buffer.address = address; slot.buffer.width = 256; slot.buffer.psm = GS_PSM_4;
    }
    for (auto &slot : d.palettes) {
        const int address = graph_vram_allocate(8, 8, GS_PSM_32, GRAPH_ALIGN_BLOCK);
        if (address < 0) return failed("NATIVE CLUT VRAM EXHAUSTED");
        slot.buffer.address = address; slot.buffer.psm = GS_PSM_32;
        slot.buffer.storage_mode = CLUT_STORAGE_MODE1; slot.buffer.load_method = CLUT_LOAD;
    }
    d.depth.enable = DRAW_ENABLE; d.depth.mask = 1; d.depth.method = ZTEST_METHOD_ALLPASS; d.depth.zsm = depth_register_format;
    if (graph_initialize(d.frames[0].address, 640, d.height, GS_PSM_32, 0, 0) < 0)
        return failed("NATIVE VIDEO MODE FAILED");
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
    qword_t *q = d.clear(frame.clear_color, frame.depth_test);
    const float sx = 640.f / frame_width, sy = float(d.height) / frame_height;
    for (const auto &triangle : frame.triangles) {
        if (q - d.packet->data > 200) { d.submit(q); q = d.packet->data; }
        const auto &material = *triangle.material;
        ++d.counters.triangles;
        q = d.test(q, material.cutout);
        if (!material.texture) {
            triangle_t solid{};
            const auto &v = triangle.vertices;
            solid.v0 = {v[0].x * sx, v[0].y * sy, reverse_depth16(v[0].depth)};
            solid.v1 = {v[1].x * sx, v[1].y * sy, reverse_depth16(v[1].depth)};
            solid.v2 = {v[2].x * sx, v[2].y * sy, reverse_depth16(v[2].depth)};
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
                std::uint64_t(fixed[k / 2].xy[(k % 2) * 2 + 1]) << 16 |
                std::uint64_t(reverse_depth16(v[k].depth)) << 32;
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
    impl_->flip(false); impl_->failure = "NONE";
    return true;
}
ReadbackStatus Display::readback_rect(unsigned x, unsigned y, unsigned width,
                                     unsigned height, std::span<std::uint32_t> pixels) {
    if (!impl_ || !impl_->ready) return ReadbackStatus::not_ready;
    auto &d = *impl_;
    const auto fail = [&d](ReadbackStatus status, const char *message) {
        d.failure = message;
        return status;
    };
    // Subtraction after origin checks prevents overflow in caller dimensions.
    if (x >= 640 || y >= d.height || !width || !height ||
        width > 640 - x || height > d.height - y)
        return fail(ReadbackStatus::invalid_rectangle, "READBACK RECTANGLE OUT OF BOUNDS");
    const std::size_t count = std::size_t(width) * height, bytes = count * 4;
    if (bytes > upload_bytes || (bytes & 127u) || pixels.size() < count || !pixels.data())
        return fail(ReadbackStatus::invalid_rectangle, "READBACK NEEDS 128-BYTE MULTIPLE <=64K AND OUTPUT SPACE");
    // This renderer owns GS and normally uses only GIF. Never steal an active
    // VIF command, FIFO, error recovery, reverse transfer or masked DMA channel.
    constexpr u32 vif_busy = 0x1f000000u | vif_reverse | 0x3f0fu;
    if ((*R_EE_D1_CHCR & dma_running) || (*R_EE_D2_CHCR & dma_running) ||
        *R_EE_D1_QWC || (VIF1_STAT & vif_busy) || (*R_EE_GIF_STAT & 0x1f001fcbu))
        return fail(ReadbackStatus::busy, "READBACK REQUIRES IDLE EXCLUSIVELY OWNED GIF/VIF1");
    const u32 dma_control = *DMA_REG_CTRL, dma_priority = *DMA_REG_PCR;
    const u32 dma_hold = *DMA_REG_ENABLER, dma_status = *DMA_REG_STAT;
    if (!(dma_control & 1u) || (dma_control & 0xcu) ||
        (dma_hold & 0x10000u) ||
        ((dma_priority & 0x80000000u) && (dma_priority & 0x60000u) != 0x60000u) ||
        (dma_status & (1u << 17))) {
        std::printf("READBACK CONFIG D_CTRL=%08lx D_PCR=%08lx D_ENABLER=%08lx D_STAT=%08lx\n",
            static_cast<unsigned long>(dma_control),static_cast<unsigned long>(dma_priority),
            static_cast<unsigned long>(dma_hold),static_cast<unsigned long>(dma_status));
        return fail(ReadbackStatus::unsupported_configuration, "READBACK DMA CONFIGURATION UNSUPPORTED");
    }

    const u32 saved_chcr = *R_EE_D1_CHCR, saved_madr = *R_EE_D1_MADR;
    const bool old_completion = (*DMA_REG_STAT & 2u) != 0;
    const auto retire = [&d, &fail](ReadbackStatus status, const char *message, bool stopped) {
        d.ready = false;
        d.transport_failed = true;
        display_quarantined = true;
        d.retain_dma_memory = !stopped;
        return fail(stopped ? status : ReadbackStatus::cleanup_failed,
                    stopped ? message : "READBACK DMA STOP FAILED; MEMORY RETAINED, RESTART REQUIRED");
    };
    // SDK VRAM addresses are32-bit words; BITBLTBUF base is256-byte blocks.
    // GS User Manual v6 section4.2.2: parameters, FINISH fence, then BUSDIR.
    qword_t *q = d.packet->data;
    PACK_GIFTAG(q, GIF_SET_TAG(5,1,0,0,GIF_FLG_PACKED,1), GIF_REG_AD); ++q;
    PACK_GIFTAG(q, GS_SET_BITBLTBUF(d.frames[d.target ^ 1].address >> 6,10,GS_PSM_32,0,0,0), GS_REG_BITBLTBUF); ++q;
    PACK_GIFTAG(q, GS_SET_TRXPOS(x,y,0,0,0), GS_REG_TRXPOS); ++q;
    PACK_GIFTAG(q, GS_SET_TRXREG(width,height), GS_REG_TRXREG); ++q;
    PACK_GIFTAG(q, GS_SET_TRXDIR(1), GS_REG_TRXDIR); ++q;
    PACK_GIFTAG(q, 0, GS_REG_FINISH); ++q;
    *GS_REG_CSR = 2; // Clear only the stale FINISH event, not vsync or SIGNAL.
    dma_channel_send_normal(DMA_CHANNEL_GIF,d.packet->data,int(q-d.packet->data),0,0);
    if (!wait_bits(R_EE_D2_CHCR,dma_running,0))
        return retire(ReadbackStatus::gif_timeout,"READBACK GIF TIMEOUT; RESTART REQUIRED",stop_owned_dma(R_EE_D2_CHCR));
    if (!wait_csr(2,2))
        return retire(ReadbackStatus::finish_timeout,"READBACK FINISH TIMEOUT; TRANSPORT UNSUPPORTED OR STALLED",true);
    *GS_REG_CSR = 2;

    auto *end = reinterpret_cast<std::uint8_t *>(d.staging) + bytes;
    SyncDCache(d.staging,end); // No dirty CPU line may overwrite incoming DMA.
    VIF1_STAT = vif_reverse;
    asm volatile("sync.l" ::: "memory");
    if (!(VIF1_STAT & vif_reverse)) {
        VIF1_STAT = 0;
        return retire(ReadbackStatus::unsupported_configuration,"READBACK VIF1 REVERSE MODE UNSUPPORTED",true);
    }
    *GS_REG_BUSDIR = 1;
    asm volatile("sync.l" ::: "memory");
    dma_channel_receive_normal(DMA_CHANNEL_VIF1,d.staging,int(bytes),0,0); // SDK takes BYTES.
    const bool completed = wait_bits(R_EE_D1_CHCR,dma_running,0);
    const bool received = completed && *R_EE_D1_QWC == 0;
    const bool stopped = completed || stop_owned_dma(R_EE_D1_CHCR);
    if (stopped) {
        InvalidDCache(d.staging,end);
        *R_EE_D1_MADR = saved_madr;
        *R_EE_D1_QWC = 0;
        *R_EE_D1_CHCR = saved_chcr;
        if (!old_completion) *DMA_REG_STAT = 2; // Preserve unrelated status/masks.
    }
    if (!received)
        return retire(ReadbackStatus::receive_timeout,"READBACK VIF1 TIMEOUT; TRANSPORT UNSUPPORTED OR STALLED",stopped);
    // Exact QWC completion accounts for every requested byte. Also observe the
    // hardware FIFO-empty states before reversing direction (GS manual p144).
    // Unsupported telemetry or a partial receive must leave the bus quarantined
    // rather than switching a nonempty FIFO. No subsequent drawing is allowed.
    if (!wait_bits(R_EE_VIF1_STAT,0x1f000000u,0) || !wait_csr(0xc000,0x4000))
        return retire(ReadbackStatus::fifo_timeout,"READBACK FIFO NOT EMPTY OR UNSUPPORTED; REVERSE BUS QUARANTINED",true);
    *GS_REG_BUSDIR = 0;
    VIF1_STAT = 0;
    asm volatile("sync.l" ::: "memory");
    std::memcpy(pixels.data(),d.staging,bytes);
    d.failure = "NONE";
    return ReadbackStatus::ok;
}
} // namespace ps2::native
