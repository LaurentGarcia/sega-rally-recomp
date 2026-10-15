// Synthetic CPU/GPU span coverage checks. No game data or ROMs required.
#include "runtime/video.h"
#include "app/gpu/gpu_renderer.h"
#include "app/gpu_device.h"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int W = rt::Video::W, H = rt::Video::H;
void require(bool condition) { if (!condition) throw std::runtime_error(SDL_GetError()); }
std::vector<uint32_t> readback(SDL_GPUDevice *device, SDL_GPUTexture *target) {
    SDL_GPUTransferBufferCreateInfo bi{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, W * H * 4, 0};
    auto *buffer = SDL_CreateGPUTransferBuffer(device, &bi); require(buffer);
    auto *cmd = SDL_AcquireGPUCommandBuffer(device); require(cmd);
    auto *pass = SDL_BeginGPUCopyPass(cmd); require(pass);
    SDL_GPUTextureRegion src{}; src.texture = target; src.w = W; src.h = H; src.d = 1;
    SDL_GPUTextureTransferInfo dst{}; dst.transfer_buffer = buffer; dst.pixels_per_row = W; dst.rows_per_layer = H;
    SDL_DownloadFromGPUTexture(pass, &src, &dst); SDL_EndGPUCopyPass(pass);
    auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd); require(fence);
    require(SDL_WaitForGPUFences(device, true, &fence, 1));
    auto *pixels = static_cast<const uint32_t *>(SDL_MapGPUTransferBuffer(device, buffer, false)); require(pixels);
    std::vector<uint32_t> result(pixels, pixels + W * H);
    SDL_UnmapGPUTransferBuffer(device, buffer); SDL_ReleaseGPUFence(device, fence); SDL_ReleaseGPUTransferBuffer(device, buffer);
    return result;
}
rt::GeoPoly polygon(const std::vector<std::array<float, 2>> &points, unsigned renderer, bool checker, bool clipped) {
    rt::GeoPoly p; p.num_vertices = uint8_t(points.size()); p.z = 100; p.luma = 252;
    p.texheader[0] = uint16_t((renderer << 13) | (checker ? 0x8000 : 0));
    p.viewport[0] = clipped ? 17 : 0; p.viewport[1] = clipped ? 184 : 0;
    p.viewport[2] = clipped ? 199 : 495; p.viewport[3] = clipped ? 367 : 384;
    p.center[1] = 384;
    for (size_t n = 0; n < points.size(); ++n) p.v[n] = {points[n][0], -points[n][1], {1.0f, 8.0f, 8.0f}};
    return p;
}
}
int main() {
    SDL_SetMainReady(); SDL_SetHint(SDL_HINT_GPU_DRIVER, "metal"); SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    require(SDL_Init(SDL_INIT_VIDEO));
    auto *device = app::create_gpu_device(); require(device);
    app::GpuRenderer gpu; require(gpu.init(device, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)); gpu.set_polygon_scanlines(true);
    SDL_GPUTextureCreateInfo ti{}; ti.type = SDL_GPU_TEXTURETYPE_2D; ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET; ti.width = W; ti.height = H; ti.layer_count_or_depth = ti.num_levels = 1;
    auto *target = SDL_CreateGPUTexture(device, &ti); require(target);
    std::vector<uint8_t> tiles(0x10000), chars(0x80000), palette(0x4000), translation(0xc000), luma(0x20000, 255);
    std::vector<uint32_t> texture(0x80000);
    palette[0x2000] = 255; palette[0x2001] = 127;
    for (size_t i = 0; i < translation.size(); i += 2)
        if ((i / 2) % 0x2000 >= 0x1f00) translation[i] = 255;
    rt::VideoMem mem{palette.data(), translation.data(), luma.data(), texture.data(), texture.data(), 0};
    rt::Video video(tiles.data(), chars.data()); video.xhout_w(uint16_t(-84)); video.xvout_w(uint16_t(-130));
    // Half-integer edges, negative clipping, crossed chains, plateaus, tips,
    // strict edge-end equality, and 3–8 vertex counts. All coordinates invented.
    const std::vector<std::vector<std::array<float, 2>>> outlines = {
        {{10.5f,10.5f},{70.5f,10.5f},{70.5f,70.5f},{10.5f,70.5f}},
        {{-4.5f,-2.5f},{45.5f,-2.5f},{45.5f,35.5f},{-4.5f,35.5f}},
        {{110.25f,14.25f},{141.75f,14.25f},{110.25f,80.75f},{141.75f,80.75f}},
        {{200.5f,10.5f},{201.5f,12.5f},{200.5f,70.5f},{199.5f,12.5f}},
        {{20.25f,100.5f},{40.5f,80.5f},{80.25f,100.5f},{80.25f,140.5f},{20.25f,140.5f}},
        {{120.5f,80.5f},{150.5f,100.5f},{165.5f,85.5f},{170.5f,140.5f},{130.5f,120.5f},{110.5f,140.5f}},
        {{200.5f,80.5f},{250.5f,80.5f},{260.5f,100.5f},{245.5f,110.5f},{260.5f,140.5f},{210.5f,140.5f},{190.5f,110.5f}},
        {{300.5f,80.5f},{320.5f,80.5f},{340.5f,100.5f},{340.5f,120.5f},{320.5f,140.5f},{300.5f,140.5f},{280.5f,120.5f},{280.5f,100.5f}},
        {{30.25f,170.25f},{30.375f,170.25f},{30.375f,190.75f},{30.25f,190.75f}},
        {{60.0f,170.5f},{70.0f,170.5f},{80.0f,170.5f},{90.0f,170.5f}},
        // Flat top/bottom, and a middle vertex exactly on a scanline centre.
        {{12.5f,12.5f},{76.5f,12.5f},{44.5f,76.5f}},
        {{100.5f,12.5f},{132.5f,76.5f},{68.5f,76.5f}},
        {{112.25f,10.25f},{172.75f,40.5f},{112.25f,80.75f}},
        // At row 30, the sloped edge is just either side of x=40.5.
        // CPU rounding and fixed subpixel triangle coverage can disagree here.
        {{20.0f,10.0f},{60.001f,50.0f},{12.0f,72.0f}},
        {{20.0f,10.0f},{59.999f,50.0f},{12.0f,72.0f}},
        {{-8.5f,-3.5f},{55.5f,25.5f},{12.5f,90.5f}},
        // Degenerate triangles have no rounded span, including determinant zero.
        {{20.5f,180.5f},{40.5f,180.5f},{60.5f,180.5f}},
        {{20.5f,120.5f},{40.5f,140.5f},{60.5f,160.5f}}
    };
    unsigned cases = 0, failures = 0;
    auto compare = [&](const std::string &label, const std::vector<rt::GeoPoly> &polys) {
        video.set_external_3d(false); video.frame_start(); video.screen_update(polys, 0, mem);
        const auto reference = video.screen();
        if (label.starts_with("layered-cutout")) {
            const auto red = std::count(reference.begin(), reference.end(), 0xffff0000u);
            const auto white = std::count(reference.begin(), reference.end(), 0xffffffffu);
            if (!red || !white) throw std::runtime_error("cutout fixture must expose both layers");
        }
        video.set_external_3d(true, true); video.frame_start(); video.screen_update(polys, 0, mem);
        auto *cmd = SDL_AcquireGPUCommandBuffer(device); require(cmd); gpu.render(cmd, target, W, H, video);
        require(SDL_SubmitGPUCommandBuffer(cmd));
        auto actual = readback(device, target);
        size_t differing = 0, first = 0;
        for (size_t i = 0; i < reference.size(); ++i) if ((reference[i] & 0xffffffu) != (actual[i] & 0xffffffu)) {
            if (!differing) first = i; ++differing;
        }
        ++cases;
        if (differing) {
            ++failures;
            std::fprintf(stderr, "%s differs=%zu first=%zu,%zu cpu=%08x gpu=%08x\n",
                         label.c_str(), differing, first % W, first / W, reference[first], actual[first]);
        }
    };
    for (size_t shape = 0; shape < outlines.size(); ++shape)
    for (bool reversed : {false, true}) for (unsigned renderer : {0u, 2u, 3u})
    for (bool checker : {false, true}) for (bool clipped : {false, true}) {
        auto points = outlines[shape]; if (reversed) std::reverse(points.begin(), points.end());
        const std::string label = "shape=" + std::to_string(shape) + " reversed=" + std::to_string(reversed) +
            " renderer=" + std::to_string(renderer) + " checker=" + std::to_string(checker) + " clipped=" + std::to_string(clipped);
        compare(label, {polygon(points, renderer, checker, clipped)});
    }
    // A transparent border surrounds opaque texture texels. Vary both UVs
    // across a front quad so transparent fragments must leave depth available
    // for a red solid behind it. Exact binary UV increments isolate cutout/
    // ordering behavior from accumulated CPU interpolation-rounding error.
    palette[0x2002] = 31;
    std::fill(texture.begin(), texture.end(), 0xffffffffu);
    for (unsigned y = 8; y < 24; ++y) for (unsigned x = 8; x < 24; ++x) {
        const unsigned offset = (y / 2) * 512 + x / 2;
        const unsigned shift = (offset & 1) * 16 + ((y & 1) ? 0 : 8) + ((x & 1) ? 0 : 4);
        texture[offset / 2] &= ~(15u << shift);
    }
    ++mem.tex_generation;
    const std::vector<std::array<float, 2>> square{{20.5f,200.5f},{84.5f,200.5f},{84.5f,264.5f},{20.5f,264.5f}};
    auto lower = polygon(square, 0, false, false); lower.z = 200; lower.texheader[3] = 64;
    auto upper = polygon(square, 3, false, false);
    for (auto &vertex : upper.v) {
        vertex.p[1] = (vertex.x - 20.5f) * 4.0f;
        vertex.p[2] = (-vertex.y - 200.5f) * 4.0f;
    }
    compare("layered-cutout", {upper, lower});
    compare("layered-cutout-reversed-submit-order", {lower, upper});
    upper.texheader[0] |= 0x8000;
    compare("layered-cutout-checker", {upper, lower});
    // Triangles use the reference plane setup rather than polygon edge
    // interpolation. Keep the same binary UV increments and assert visible
    // contributions from BOTH layers, including checker holes, in each winding.
    for (bool reversed : {false, true}) for (bool clipped_left : {false, true}) {
        std::vector<std::array<float, 2>> triangle{{20.5f,200.5f},{84.5f,200.5f},{20.5f,264.5f}};
        if (reversed) std::reverse(triangle.begin(), triangle.end());
        auto triangle_lower = polygon(triangle, 0, false, false);
        triangle_lower.z = 200; triangle_lower.texheader[3] = 64;
        auto triangle_upper = polygon(triangle, 3, false, false);
        if (clipped_left) triangle_lower.viewport[0] = triangle_upper.viewport[0] = 37;
        for (auto &vertex : triangle_upper.v) {
            vertex.p[1] = (vertex.x - 20.5f) * 4.0f;
            vertex.p[2] = (-vertex.y - 200.5f) * 4.0f;
        }
        const std::string label = "layered-cutout-triangle-winding=" + std::to_string(reversed) +
            "-clipped-left=" + std::to_string(clipped_left);
        compare(label, {triangle_upper, triangle_lower});
        compare(label + "-reversed-submit-order", {triangle_lower, triangle_upper});
        triangle_upper.texheader[0] |= 0x8000;
        compare(label + "-checker", {triangle_upper, triangle_lower});
    }
    std::printf("synthetic polygon spans: cases=%u failures=%u pixels_compared=%llu\n", cases, failures,
                static_cast<unsigned long long>(cases) * W * H);
    require(SDL_WaitForGPUIdle(device)); gpu.shutdown(); SDL_ReleaseGPUTexture(device, target); SDL_DestroyGPUDevice(device); SDL_Quit();
    return failures ? 1 : 0;
}
