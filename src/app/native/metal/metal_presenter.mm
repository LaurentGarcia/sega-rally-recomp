#include "app/native/metal/metal_presenter.h"
#include "app/native/metal/model2_metal.h"
#include "app/native/metal/metal_crt.h"
#include "app/segarally_display.h"
#include "app/gpu/shaders_gen.h"
#include "runtime/video.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <vector>

namespace app::native {
namespace {
constexpr int width = 496, height = 384;
std::string describe(NSError *error) {
    return error ? std::string(error.localizedDescription.UTF8String) : "unknown Metal error";
}
}
struct MetalPresenter::Impl {
    SDL_Window *window = nullptr;
    SDL_MetalView view = nullptr;
    CAMetalLayer *layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLTexture> screen = nil;
    id<MTLRenderPipelineState> plain = nil;
    id<MTLSamplerState> nearest = nil, linear = nil;
    std::deque<id<MTLCommandBuffer>> pending;
    MetalModel2 model2;
    MetalCrt crt;
    bool screen_ready = false, accurate_textures = false;
    Stats counts;
    std::string error;

    bool retire(bool all) {
        bool ok = true;
        while (!pending.empty()) {
            auto command = pending.front();
            if (!all && pending.size() < 2 && command.status != MTLCommandBufferStatusCompleted &&
                command.status != MTLCommandBufferStatusError) break;
            [command waitUntilCompleted];
            if (command.status != MTLCommandBufferStatusCompleted) {
                error = "Metal submission: " + describe(command.error); ok = false;
            }
            pending.pop_front();
        }
        return ok;
    }
    bool make_plain() {
        NSError *failure = nil;
        auto vertex = [device newLibraryWithSource:@(k_vs_quad_msl) options:nil error:&failure];
        if (!vertex) { error = "Metal presentation vertex: " + describe(failure); return false; }
        auto fragment = [device newLibraryWithSource:@(k_ps_quad_msl) options:nil error:&failure];
        if (!fragment) { error = "Metal presentation fragment: " + describe(failure); return false; }
        auto *desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = [vertex newFunctionWithName:@"main0"];
        desc.fragmentFunction = [fragment newFunctionWithName:@"main0"];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        auto *layout = [MTLVertexDescriptor vertexDescriptor];
        for (NSUInteger i = 0; i < 2; ++i) {
            layout.attributes[i].format = MTLVertexFormatFloat2;
            layout.attributes[i].offset = i * sizeof(float) * 2;
            layout.attributes[i].bufferIndex = 1;
        }
        layout.layouts[1].stride = sizeof(float) * 4;
        layout.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;
        layout.layouts[1].stepRate = 1;
        desc.vertexDescriptor = layout;
        plain = [device newRenderPipelineStateWithDescriptor:desc error:&failure];
        if (!plain) { error = "Metal presentation pipeline: " + describe(failure); return false; }
        auto *sampler = [MTLSamplerDescriptor new];
        sampler.sAddressMode = sampler.tAddressMode = sampler.rAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterNearest;
        sampler.mipFilter = MTLSamplerMipFilterNearest;
        nearest = [device newSamplerStateWithDescriptor:sampler];
        sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterLinear;
        linear = [device newSamplerStateWithDescriptor:sampler];
        if (!nearest || !linear) { error = "Metal presentation sampler creation failed"; return false; }
        return true;
    }
    bool present_plain(id<MTLCommandBuffer> command, id<MTLTexture> target, PixelViewport viewport, bool smooth) {
        auto *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);
        auto encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) { error = "Metal presentation encoder creation failed"; return false; }
        encoder.label = @"Sega Rally original display";
        [encoder setRenderPipelineState:plain]; [encoder setCullMode:MTLCullModeNone];
        [encoder setViewport:MTLViewport{double(viewport.x),double(viewport.y),double(viewport.w),double(viewport.h),0,1}];
        [encoder setScissorRect:MTLScissorRect{viewport.x,viewport.y,viewport.w,viewport.h}];
        const float screen_size[4] = {float(width),float(height),0,0};
        const float quad[6][4] = {{0,0,0,0},{width,0,1,0},{0,height,0,1},
                                  {width,0,1,0},{width,height,1,1},{0,height,0,1}};
        [encoder setVertexBytes:screen_size length:sizeof screen_size atIndex:0];
        [encoder setVertexBytes:quad length:sizeof quad atIndex:1];
        [encoder setFragmentTexture:screen atIndex:0];
        [encoder setFragmentSamplerState:smooth ? linear : nearest atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        [encoder endEncoding]; return true;
    }
};

MetalPresenter::MetalPresenter() : impl_(std::make_unique<Impl>()) {}
MetalPresenter::~MetalPresenter() { shutdown(); }
const std::string &MetalPresenter::error() const { return impl_->error; }
MetalPresenter::Stats MetalPresenter::stats() const { return impl_->counts; }
void *MetalPresenter::metal_device() const { return (__bridge void *)impl_->device; }
bool MetalPresenter::wait_idle() { return impl_->retire(true); }
void MetalPresenter::shutdown() {
    auto &s = *impl_; s.retire(true);
    s.model2.shutdown(); s.crt.shutdown(); s.plain = nil; s.nearest = s.linear = nil; s.screen = nil;
    s.queue = nil; s.layer = nil; s.device = nil;
    if (s.view) SDL_Metal_DestroyView(s.view);
    s.view = nullptr; s.window = nullptr; s.screen_ready = false;
}
bool MetalPresenter::init(SDL_Window *window) { @autoreleasepool {
    shutdown(); auto &s = *impl_; s.error.clear(); s.counts = {};
    if (!window) { s.error = "Metal presenter requires a window"; return false; }
    s.window = window; s.device = MTLCreateSystemDefaultDevice();
    if (!s.device) { s.error = "Metal device unavailable"; shutdown(); return false; }
    s.view = SDL_Metal_CreateView(window);
    if (!s.view) { s.error = std::string("Metal window view: ") + SDL_GetError(); shutdown(); return false; }
    s.layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(s.view);
    if (!s.layer) { s.error = "SDL Metal view has no CAMetalLayer"; shutdown(); return false; }
    s.layer.device = s.device; s.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s.layer.framebufferOnly = YES; s.layer.opaque = YES; s.layer.allowsNextDrawableTimeout = YES;
    s.layer.displaySyncEnabled = YES;
    s.layer.maximumDrawableCount = 3;
    s.queue = [s.device newCommandQueue];
    auto *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
        width:width height:height mipmapped:NO];
    desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    s.screen = [s.device newTextureWithDescriptor:desc];
    if (!s.queue || !s.screen) { s.error = "Metal queue or game texture creation failed"; shutdown(); return false; }
    if (!s.model2.init(s.device,MTLPixelFormatBGRA8Unorm)) { s.error = s.model2.error(); shutdown(); return false; }
    if (!s.crt.init(s.device,MTLPixelFormatBGRA8Unorm)) { s.error = s.crt.error(); shutdown(); return false; }
    if (!s.make_plain()) { shutdown(); return false; }
    return true;
}}
bool MetalPresenter::render(const rt::Video &video, DisplaySettings &settings, bool update_game,
                             bool has_game, DisplayPanel &panel, const std::function<void()> &overlay) { @autoreleasepool {
    auto &s = *impl_;
    if (!s.queue || !s.window) { s.error = "Metal presenter is not initialized"; return false; }
    if (!s.retire(false)) return false;
    id<CAMetalDrawable> drawable = nil;
    int pixel_w = 0, pixel_h = 0;
    if (!SDL_GetWindowSizeInPixels(s.window,&pixel_w,&pixel_h)) {
        s.error = std::string("Metal window size: ") + SDL_GetError(); return false;
    }
    const auto flags = SDL_GetWindowFlags(s.window);
    if (!(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED)) && pixel_w > 0 && pixel_h > 0) {
        s.layer.drawableSize = CGSizeMake(pixel_w,pixel_h);
        drawable = [s.layer nextDrawable];
    }
    MTLRenderPassDescriptor *ui_pass = nil;
    if (drawable) {
        ui_pass = [MTLRenderPassDescriptor renderPassDescriptor];
        ui_pass.colorAttachments[0].texture = drawable.texture;
        ui_pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        ui_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        if (!panel.new_frame_metal(settings,(__bridge void *)ui_pass,overlay)) { s.error = panel.error(); return false; }
    } else ++s.counts.skipped_drawables;
    auto command = [s.queue commandBuffer];
    if (!command) { s.error = "Metal command buffer unavailable"; return false; }
    command.label = @"Sega Rally player frame";
    if (update_game || (has_game && s.screen_ready && settings.accurate_textures != s.accurate_textures)) {
        s.model2.set_polygon_scanlines(settings.accurate_textures);
        if (!s.model2.encode(command,s.screen,width,height,video)) { s.error = s.model2.error(); return false; }
        s.accurate_textures = settings.accurate_textures; s.screen_ready = true; ++s.counts.game_frames;
    }
    if (drawable) {
        const bool show_game = has_game && s.screen_ready;
        if (show_game) {
            const auto target = drawable.texture;
            float scale = std::min(float(target.width) / width,float(target.height) / height);
            if (settings.integer_scale && scale >= 1) scale = std::floor(scale);
            const uint32_t w = std::max(1u,uint32_t(width * scale)), h = std::max(1u,uint32_t(height * scale));
            const PixelViewport viewport{uint32_t((target.width-w)/2),uint32_t((target.height-h)/2),w,h};
            if (settings.crt.enabled) {
                if (!s.crt.encode(command,s.screen,target,viewport,settings.crt,{width,height})) {
                    s.error = s.crt.error(); return false;
                }
            } else if (!s.present_plain(command,target,viewport,settings.linear_filter)) return false;
        } else {
            ui_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            ui_pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);
        }
        auto encoder = [command renderCommandEncoderWithDescriptor:ui_pass];
        if (!encoder) { s.error = "Metal Settings encoder unavailable"; return false; }
        encoder.label = @"Sega Rally Settings and controls";
        if (!panel.render_metal((__bridge void *)command,(__bridge void *)encoder)) {
            [encoder endEncoding]; s.error = panel.error(); return false;
        }
        [encoder endEncoding]; [command presentDrawable:drawable]; ++s.counts.presented_frames;
    }
    [command commit]; s.pending.push_back(command); return true;
}}
bool MetalPresenter::capture(const std::filesystem::path &path) { @autoreleasepool {
    auto &s = *impl_;
    if (!s.screen_ready || !s.screen) { s.error = "Metal capture has no game frame"; return false; }
    if (!wait_idle()) return false;
    std::vector<uint32_t> pixels(size_t(width) * height);
    [s.screen getBytes:pixels.data() bytesPerRow:width * 4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
    std::ofstream file(path,std::ios::binary);
    if (!file) { s.error = "Cannot open Metal capture: " + path.string(); return false; }
    file << "P6\n" << width << " " << height << "\n255\n";
    for (const auto pixel : pixels) {
        const char rgb[] = {char(pixel >> 16),char(pixel >> 8),char(pixel)};
        file.write(rgb,sizeof rgb);
    }
    file.flush();
    if (!file) { s.error = "Cannot write Metal capture: " + path.string(); return false; }
    uint64_t hash = 0xcbf29ce484222325ULL;
    const auto *bytes = reinterpret_cast<const uint8_t *>(pixels.data());
    for (size_t i = 0; i < pixels.size() * 4; ++i) { hash ^= bytes[i]; hash *= 0x100000001b3ULL; }
    std::fprintf(stderr,"GPU frame capture: %s BGRA hash=%016llx\n",path.string().c_str(),
        static_cast<unsigned long long>(hash));
    return true;
}}
} // namespace app::native
