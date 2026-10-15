// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels, Ville Linde, Aaron Giles
// Polygon edge setup adapts runtime/raster.cpp's pinned MAME poly.h port.
#include "app/gpu/gpu_renderer.h"

#include "app/gpu/shaders_gen.h"
#include "app/gpu/model2_resources.h"
#include "runtime/model2_priority.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <numeric>

namespace app {

namespace {

constexpr uint32_t kSheetWords = model2_resources::sheet_words;
constexpr uint32_t kTexramBytes = model2_resources::texram_bytes;
constexpr uint32_t kLumaBytes = model2_resources::luma_bytes;
constexpr uint32_t kXlatEntries = model2_resources::xlat_entries;
constexpr uint32_t kTilePixBytes = model2_resources::tilepix_bytes;
constexpr uint32_t kTileDataWords = model2_resources::tiledata_words;

} // namespace

SDL_GPUShader *GpuRenderer::shader(const unsigned char *spv, size_t spv_len, const unsigned char *dxil, size_t dxil_len,
                                   const char *msl, const char *dxil_entry, SDL_GPUShaderStage stage, int uniforms,
                                   int samplers, int storage_buffers) {
    SDL_GPUShaderCreateInfo si{};
    si.stage = stage;
    si.num_uniform_buffers = Uint32(uniforms);
    si.num_samplers = Uint32(samplers);
    si.num_storage_buffers = Uint32(storage_buffers);
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(dev_);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        si.format = SDL_GPU_SHADERFORMAT_SPIRV, si.code = spv, si.code_size = spv_len, si.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        si.format = SDL_GPU_SHADERFORMAT_DXIL, si.code = dxil, si.code_size = dxil_len, si.entrypoint = dxil_entry;
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        si.format = SDL_GPU_SHADERFORMAT_MSL, si.code = reinterpret_cast<const Uint8 *>(msl),
        si.code_size = std::strlen(msl), si.entrypoint = "main0";
    } else {
        error_ = "no supported shader format (SPIR-V, DXIL or MSL)";
        return nullptr;
    }
    SDL_GPUShader *s = SDL_CreateGPUShader(dev_, &si);
    if (!s) error_ = std::string("SDL_CreateGPUShader: ") + SDL_GetError();
    return s;
}

bool GpuRenderer::init(SDL_GPUDevice *dev, SDL_GPUTextureFormat format) {
    dev_ = dev;
    const char *scanlines = std::getenv("SEGARALLY_GPU_POLYGON_SCANLINES");
    if (scanlines && std::strcmp(scanlines, "1") == 0) polygon_scanlines_ = true;

    SDL_GPUShader *vs_poly = shader(k_vs_poly_spv, sizeof k_vs_poly_spv, k_vs_poly_dxil, sizeof k_vs_poly_dxil,
                                    k_vs_poly_msl, "vs_poly", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *ps_poly = shader(k_ps_poly_spv, sizeof k_ps_poly_spv, k_ps_poly_dxil, sizeof k_ps_poly_dxil,
                                    k_ps_poly_msl, "ps_poly", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 4);
    SDL_GPUShader *vs_quad = shader(k_vs_quad_spv, sizeof k_vs_quad_spv, k_vs_quad_dxil, sizeof k_vs_quad_dxil,
                                    k_vs_quad_msl, "vs_quad", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *ps_quad = shader(k_ps_quad_spv, sizeof k_ps_quad_spv, k_ps_quad_dxil, sizeof k_ps_quad_dxil,
                                    k_ps_quad_msl, "ps_quad", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    SDL_GPUShader *ps_back = shader(k_ps_tiles_back_spv, sizeof k_ps_tiles_back_spv, k_ps_tiles_back_dxil,
                                    sizeof k_ps_tiles_back_dxil, k_ps_tiles_back_msl, "ps_tiles_back",
                                    SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 2);
    SDL_GPUShader *ps_front = shader(k_ps_tiles_front_spv, sizeof k_ps_tiles_front_spv, k_ps_tiles_front_dxil,
                                     sizeof k_ps_tiles_front_dxil, k_ps_tiles_front_msl, "ps_tiles_front",
                                     SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 2);
    if (!vs_poly || !ps_poly || !vs_quad || !ps_quad || !ps_back || !ps_front) {
        for (SDL_GPUShader *s : {vs_poly, ps_poly, vs_quad, ps_quad, ps_back, ps_front})
            if (s) SDL_ReleaseGPUShader(dev_, s);
        return false;
    }

    SDL_GPUColorTargetDescription color{};
    color.format = format;

    // Polygons: position, depth (draw order), 1/z u/z v/z, colour, texture state.
    SDL_GPUVertexBufferDescription pvb{};
    pvb.slot = 0;
    pvb.pitch = sizeof(PolyVertex);
    pvb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute pattr[7] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Uint32(offsetof(PolyVertex, x))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, Uint32(offsetof(PolyVertex, depth))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, Uint32(offsetof(PolyVertex, ooz))},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, Uint32(offsetof(PolyVertex, r))},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, Uint32(offsetof(PolyVertex, info))},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT2, Uint32(offsetof(PolyVertex, more))},
        {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT2, Uint32(offsetof(PolyVertex, edges))},
    };
    SDL_GPUGraphicsPipelineCreateInfo pi{};
    pi.vertex_shader = vs_poly;
    pi.fragment_shader = ps_poly;
    pi.vertex_input_state.vertex_buffer_descriptions = &pvb;
    pi.vertex_input_state.num_vertex_buffers = 1;
    pi.vertex_input_state.vertex_attributes = pattr;
    pi.vertex_input_state.num_vertex_attributes = 7;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS; // earlier in the order wins
    pi.target_info.color_target_descriptions = &color;
    pi.target_info.num_color_targets = 1;
    pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    pi.target_info.has_depth_stencil_target = true;
    poly_pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);

    // Layers: a textured quad; no depth test.
    SDL_GPUVertexBufferDescription qvb{};
    qvb.slot = 0;
    qvb.pitch = sizeof(QuadVertex);
    qvb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute qattr[2] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Uint32(offsetof(QuadVertex, x))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Uint32(offsetof(QuadVertex, u))},
    };
    SDL_GPUGraphicsPipelineCreateInfo qi{};
    qi.vertex_shader = vs_quad;
    qi.fragment_shader = ps_quad;
    qi.vertex_input_state.vertex_buffer_descriptions = &qvb;
    qi.vertex_input_state.num_vertex_buffers = 1;
    qi.vertex_input_state.vertex_attributes = qattr;
    qi.vertex_input_state.num_vertex_attributes = 2;
    qi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    qi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    qi.target_info.color_target_descriptions = &color;
    qi.target_info.num_color_targets = 1;
    qi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    qi.target_info.has_depth_stencil_target = true; // same pass as the polygons
    quad_pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &qi);
    // Tilemap layers: the same quad, composed per pixel.
    qi.fragment_shader = ps_back;
    back_pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &qi);
    qi.fragment_shader = ps_front;
    front_pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &qi);

    for (SDL_GPUShader *s : {vs_poly, ps_poly, vs_quad, ps_quad, ps_back, ps_front}) SDL_ReleaseGPUShader(dev_, s);
    if (!poly_pipe_ || !quad_pipe_ || !back_pipe_ || !front_pipe_) {
        error_ = std::string("SDL_CreateGPUGraphicsPipeline: ") + SDL_GetError();
        shutdown();
        return false;
    }

    SDL_GPUSamplerCreateInfo smp{}; // layers are drawn 1:1: nearest
    smp.min_filter = smp.mag_filter = SDL_GPU_FILTER_NEAREST;
    smp.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    smp.address_mode_u = smp.address_mode_v = smp.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(dev_, &smp);

    SDL_GPUBufferCreateInfo qb{};
    qb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    qb.size = sizeof(QuadVertex) * 6;
    qbuf_ = SDL_CreateGPUBuffer(dev_, &qb);
    SDL_GPUBufferCreateInfo sb{};
    sb.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    sb.size = kTexramBytes;
    texram_ = SDL_CreateGPUBuffer(dev_, &sb);
    sb.size = kLumaBytes;
    luma_ = SDL_CreateGPUBuffer(dev_, &sb);
    sb.size = kXlatEntries * 4;
    xlat_ = SDL_CreateGPUBuffer(dev_, &sb);
    sb.size = kTilePixBytes;
    tilepix_ = SDL_CreateGPUBuffer(dev_, &sb);
    sb.size = kTileDataWords * 4;
    tiledata_ = SDL_CreateGPUBuffer(dev_, &sb);
    sb.size = sizeof(PolyEdge);
    edges_ = SDL_CreateGPUBuffer(dev_, &sb);
    edges_size_ = sb.size;
    if (!sampler_ || !qbuf_ || !texram_ || !luma_ || !xlat_ || !tilepix_ || !tiledata_ || !edges_) {
        error_ = std::string("SDL_GPU setup: ") + SDL_GetError();
        shutdown();
        return false;
    }
    return true;
}

void GpuRenderer::shutdown() {
    if (!dev_) return;
    if (poly_pipe_) SDL_ReleaseGPUGraphicsPipeline(dev_, poly_pipe_), poly_pipe_ = nullptr;
    for (SDL_GPUGraphicsPipeline **p : {&quad_pipe_, &back_pipe_, &front_pipe_})
        if (*p) SDL_ReleaseGPUGraphicsPipeline(dev_, *p), *p = nullptr;
    if (sampler_) SDL_ReleaseGPUSampler(dev_, sampler_), sampler_ = nullptr;
    if (front_) SDL_ReleaseGPUTexture(dev_, front_), front_ = nullptr;
    if (depth_) SDL_ReleaseGPUTexture(dev_, depth_), depth_ = nullptr;
    if (vbuf_) SDL_ReleaseGPUBuffer(dev_, vbuf_), vbuf_ = nullptr;
    if (qbuf_) SDL_ReleaseGPUBuffer(dev_, qbuf_), qbuf_ = nullptr;
    for (SDL_GPUBuffer **b : {&texram_, &luma_, &xlat_, &tilepix_, &tiledata_, &edges_})
        if (*b) SDL_ReleaseGPUBuffer(dev_, *b), *b = nullptr;
    texram_generation_ = ~0ull;
    tile_instance_ = 0;
    if (upload_) SDL_ReleaseGPUTransferBuffer(dev_, upload_), upload_ = nullptr;
    vbuf_size_ = upload_size_ = 0;
    edges_size_ = 0;
    depth_w_ = depth_h_ = front_w_ = front_h_ = 0;
}

// Textures and buffers big enough for this frame.
bool GpuRenderer::ensure(int w, int h, int scale, uint32_t vert_bytes, uint32_t edge_bytes) {
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    if (w != front_w_ || h != front_h_) {
        if (front_) SDL_ReleaseGPUTexture(dev_, front_), front_ = nullptr;
        ti.width = Uint32(w);
        ti.height = Uint32(h);
        ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM; // the layers' 0xAARRGGBB words
        ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        front_ = SDL_CreateGPUTexture(dev_, &ti);
        if (!front_) return false;
        front_w_ = w, front_h_ = h;
    }
    if (w * scale != depth_w_ || h * scale != depth_h_) {
        if (depth_) SDL_ReleaseGPUTexture(dev_, depth_), depth_ = nullptr;
        ti.width = Uint32(w * scale);
        ti.height = Uint32(h * scale);
        ti.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depth_ = SDL_CreateGPUTexture(dev_, &ti);
        if (!depth_) return false;
        depth_w_ = w * scale, depth_h_ = h * scale;
    }
    if (vert_bytes > vbuf_size_) {
        if (vbuf_) SDL_ReleaseGPUBuffer(dev_, vbuf_);
        vbuf_size_ = std::max<uint32_t>(vert_bytes, 1u << 20) * 2;
        SDL_GPUBufferCreateInfo bi{};
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bi.size = vbuf_size_;
        vbuf_ = SDL_CreateGPUBuffer(dev_, &bi);
        if (!vbuf_) return false;
    }
    if (edge_bytes > edges_size_) {
        SDL_ReleaseGPUBuffer(dev_, edges_);
        edges_size_ = std::max(edge_bytes, 1u << 16) * 2;
        SDL_GPUBufferCreateInfo bi{};
        bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        bi.size = edges_size_;
        edges_ = SDL_CreateGPUBuffer(dev_, &bi);
        if (!edges_) return false;
    }
    const uint32_t need = uint32_t(w) * uint32_t(h) * 4 + uint32_t(sizeof(QuadVertex) * 6) + vert_bytes + edge_bytes +
                          kTexramBytes + kLumaBytes + kXlatEntries * 4 + kTilePixBytes + kTileDataWords * 4;
    if (need > upload_size_) {
        if (upload_) SDL_ReleaseGPUTransferBuffer(dev_, upload_);
        upload_size_ = need * 2;
        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tb.size = upload_size_;
        upload_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
        if (!upload_) return false;
    }
    return true;
}

// The polygons as triangles, in the rasterizer's order, batched by clip rectangle.
void GpuRenderer::render(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, int w, int h, const rt::Video &video,
                         int scale) {
    if (!ok()) return;
    scale = std::clamp(scale, 1, 4);
    build(video, w, h, scale);
    const uint32_t vert_bytes = uint32_t(verts_.size() * sizeof(PolyVertex));
    const uint32_t edge_bytes = uint32_t(poly_edges_.size() * sizeof(PolyEdge));
    if (!ensure(w, h, scale, vert_bytes, edge_bytes)) return;

    // The tilemap pixmaps' rows to upload, per layer: the span of the tile
    // rows changed since tilepix_ was filled (everything for a new Video).
    const uint64_t tile_generation = video.system24_texture_generation();
    const bool all_tiles = video.instance() != tile_instance_ || tile_generation < tile_generation_;
    for (int l = 0; l < 4; l++) {
        int first = 64, last = -1;
        for (unsigned r = 0; r < 64; r++) {
            bool changed = all_tiles;
            for (unsigned t = r * 64; !changed && t < r * 64 + 64; t++)
                changed = video.system24_tile_generation(l, t) > tile_generation_;
            if (changed) first = std::min(first, int(r)), last = int(r);
        }
        tile_rows_[l] = last < 0 ? Rows{0, 0} : Rows{uint32_t(first) * 8, uint32_t(last - first + 1) * 8};
    }
    tile_instance_ = video.instance();
    tile_generation_ = tile_generation;

    // upload: the CPU front layers (HUD at the edges only), the layer quad,
    // the polygon vertices, the storage data
    const bool cpu_front = video.cpu_front();
    const uint32_t layer_bytes = uint32_t(w) * uint32_t(h) * 4;
    auto *p = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(dev_, upload_, true));
    uint32_t at = 0;
    if (cpu_front) {
        const std::vector<uint32_t> &front = video.foreground_layer();
        std::memcpy(p, front.data(), std::min(front.size() * 4, size_t(layer_bytes)));
        at = layer_bytes;
    }
    const float fw = float(w), fh = float(h);
    const QuadVertex quad[6] = {{0, 0, 0, 0}, {fw, 0, 1, 0}, {0, fh, 0, 1}, {fw, 0, 1, 0}, {fw, fh, 1, 1}, {0, fh, 0, 1}};
    const uint32_t quad_at = at;
    std::memcpy(p + quad_at, quad, sizeof quad);
    const uint32_t verts_at = quad_at + uint32_t(sizeof quad);
    if (vert_bytes) std::memcpy(p + verts_at, verts_.data(), vert_bytes);
    const uint32_t edges_at = verts_at + vert_bytes;
    if (edge_bytes) std::memcpy(p + edges_at, poly_edges_.data(), edge_bytes);
    at = edges_at + edge_bytes;
    // luma RAM and the gamma'd colour translation every frame, texture RAM when it changed
    const rt::VideoMem &mem = video.gpu_mem();
    const uint32_t data_at = at;
    const bool have_mem = mem.lumaram && mem.colorxlat && mem.tex0 && mem.tex1;
    bool tex_changed = false;
    if (have_mem) {
        std::memcpy(p + data_at, mem.lumaram, kLumaBytes);
        xlat_table_.resize(kXlatEntries);
        model2_resources::pack_xlat(mem.colorxlat, gamma_, xlat_table_.data());
        std::memcpy(p + data_at + kLumaBytes, xlat_table_.data(), kXlatEntries * 4);
        at += kLumaBytes + kXlatEntries * 4;
        tex_changed = mem.tex_generation != texram_generation_; // the board counts texture RAM writes
        if (tex_changed) {
            texram_generation_ = mem.tex_generation;
            std::memcpy(p + at, mem.tex0, kSheetWords * 4);
            std::memcpy(p + at + kSheetWords * 4, mem.tex1, kSheetWords * 4);
            at += kTexramBytes;
        }
    }
    // tilemaps: the changed pixmap rows, packed as m2.hlsl reads them (pen, category in bit 15)
    uint32_t rows_at[4];
    for (int l = 0; l < 4; l++) {
        rows_at[l] = at;
        const uint32_t n = tile_rows_[l].count * 512;
        if (!n) continue;
        uint16_t *dst = reinterpret_cast<uint16_t *>(p + at);
        model2_resources::pack_tile_rows(video, unsigned(l), tile_rows_[l].first, tile_rows_[l].count, dst);
        at += n * 2;
    }
    // the header (widescreen margin and its fill), pens, tile words
    tiledata_words_.resize(kTileDataWords);
    model2_resources::pack_tile_data(video, scale, tiledata_words_.data());
    const uint32_t tiledata_at = at;
    std::memcpy(p + tiledata_at, tiledata_words_.data(), kTileDataWords * 4);
    SDL_UnmapGPUTransferBuffer(dev_, upload_);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    // cycle: the whole buffer is replaced, so a buffer still in use may be swapped for a fresh one
    auto upload_buffer = [&](uint32_t from, SDL_GPUBuffer *buffer, uint32_t offset, uint32_t size, bool cycle = true) {
        SDL_GPUTransferBufferLocation src{upload_, from};
        SDL_GPUBufferRegion dst{buffer, offset, size};
        SDL_UploadToGPUBuffer(copy, &src, &dst, cycle);
    };
    if (cpu_front) {
        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = upload_;
        src.pixels_per_row = Uint32(w);
        SDL_GPUTextureRegion dst{};
        dst.texture = front_;
        dst.w = Uint32(w);
        dst.h = Uint32(h);
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, true);
    }
    upload_buffer(quad_at, qbuf_, 0, sizeof quad);
    if (vert_bytes) upload_buffer(verts_at, vbuf_, 0, vert_bytes);
    if (edge_bytes) upload_buffer(edges_at, edges_, 0, edge_bytes);
    if (have_mem) {
        upload_buffer(data_at, luma_, 0, kLumaBytes);
        upload_buffer(data_at + kLumaBytes, xlat_, 0, kXlatEntries * 4);
        if (tex_changed) upload_buffer(data_at + kLumaBytes + kXlatEntries * 4, texram_, 0, kTexramBytes);
    }
    for (int l = 0; l < 4; l++)
        if (tile_rows_[l].count)
            upload_buffer(rows_at[l], tilepix_, (uint32_t(l) * 512 * 512 + tile_rows_[l].first * 512) * 2,
                          tile_rows_[l].count * 512 * 2, false); // rows: the rest must stay
    upload_buffer(tiledata_at, tiledata_, 0, kTileDataWords * 4);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUColorTargetInfo ct{};
    ct.texture = target;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.clear_color = SDL_FColor{0, 0, 0, 1};
    ct.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo dt{};
    dt.texture = depth_;
    dt.clear_depth = 1.0f;
    dt.load_op = SDL_GPU_LOADOP_CLEAR;
    dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
    // the target may be bigger than this frame; positions are in original
    // pixels (screen), the viewport and clip rectangles in the target's
    const SDL_GPUViewport viewport{0, 0, float(w * scale), float(h * scale), 0, 1};
    SDL_SetGPUViewport(pass, &viewport);
    const float screen[4] = {fw, fh, 0, 0};
    SDL_PushGPUVertexUniformData(cmd, 0, screen, sizeof screen);
    const SDL_Rect full{0, 0, w * scale, h * scale};

    // a full-frame quad through `pipe`: the tilemap layers, or the CPU front layers
    auto draw_quad = [&](SDL_GPUGraphicsPipeline *pipe) {
        SDL_BindGPUGraphicsPipeline(pass, pipe);
        SDL_SetGPUScissor(pass, &full);
        SDL_GPUBufferBinding qb{qbuf_, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &qb, 1);
        if (pipe == quad_pipe_) {
            SDL_GPUTextureSamplerBinding tsb{front_, sampler_};
            SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
        } else {
            SDL_GPUBuffer *storage[2] = {tilepix_, tiledata_};
            SDL_BindGPUFragmentStorageBuffers(pass, 0, storage, 2);
        }
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    };
    draw_quad(back_pipe_);
    if (!verts_.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, poly_pipe_);
        SDL_GPUBufferBinding vb{vbuf_, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_GPUBuffer *storage[4] = {texram_, luma_, xlat_, edges_};
        SDL_BindGPUFragmentStorageBuffers(pass, 0, storage, 4);
        for (const Batch &b : batches_) {
            if (!b.count) continue;
            const SDL_Rect clip{b.clip.x * scale, b.clip.y * scale, b.clip.w * scale, b.clip.h * scale};
            SDL_SetGPUScissor(pass, &clip);
            SDL_DrawGPUPrimitives(pass, b.count, 1, b.first, 0);
        }
    }
    draw_quad(cpu_front ? quad_pipe_ : front_pipe_);
    SDL_EndGPURenderPass(pass);
}

} // namespace app
