#include "app/native/metal/model2_metal.h"
#include "app/gpu/model2_frame.h"
#include "app/gpu/model2_resources.h"
#include "app/gpu/shaders_gen.h"
#include <array>
#include <cstring>

namespace app::native {
namespace {
struct QuadVertex { float x, y, u, v; };
std::string describe(NSError *error) {
    return error ? std::string(error.localizedDescription.UTF8String) : "unknown Metal error";
}
MTLVertexDescriptor *vertex_layout(bool polygon) {
    auto *layout = [MTLVertexDescriptor vertexDescriptor];
    // Generated MSL reserves vertex buffer 0 for ScreenUBO.
    constexpr NSUInteger slot = 1;
    if (polygon) {
        using V = Model2Frame::PolyVertex;
        const MTLVertexFormat formats[] = {MTLVertexFormatFloat2, MTLVertexFormatFloat, MTLVertexFormatFloat3,
            MTLVertexFormatFloat4, MTLVertexFormatUInt4, MTLVertexFormatUInt2, MTLVertexFormatUInt2};
        const NSUInteger offsets[] = {offsetof(V,x), offsetof(V,depth), offsetof(V,ooz), offsetof(V,r),
            offsetof(V,info), offsetof(V,more), offsetof(V,edges)};
        for (NSUInteger i = 0; i < 7; ++i) {
            layout.attributes[i].format = formats[i];
            layout.attributes[i].offset = offsets[i];
            layout.attributes[i].bufferIndex = slot;
        }
        layout.layouts[slot].stride = sizeof(V);
    } else {
        for (NSUInteger i = 0; i < 2; ++i) {
            layout.attributes[i].format = MTLVertexFormatFloat2;
            layout.attributes[i].offset = i * sizeof(float) * 2;
            layout.attributes[i].bufferIndex = slot;
        }
        layout.layouts[slot].stride = sizeof(QuadVertex);
    }
    layout.layouts[slot].stepFunction = MTLVertexStepFunctionPerVertex;
    layout.layouts[slot].stepRate = 1;
    return layout;
}
} // namespace

struct MetalModel2::Impl {
    id<MTLDevice> device = nil;
    id<MTLRenderPipelineState> polygon = nil, back = nil, front = nil, quad = nil;
    id<MTLDepthStencilState> depth_test = nil, no_depth = nil;
    id<MTLSamplerState> sampler = nil;
    MTLPixelFormat format = MTLPixelFormatInvalid;
    Model2Frame frame;
    const std::array<uint8_t,256> gamma = model2_resources::make_gamma();
    std::string error;

    id<MTLRenderPipelineState> pipeline(const char *vertex, const char *fragment, bool poly) {
        NSError *failure = nil;
        // The same compiler defaults as the pinned SDL Metal backend; shader
        // source is the existing generated array, never rewritten for Metal.
        auto vs = [device newLibraryWithSource:@(vertex) options:nil error:&failure];
        if (!vs) { error = "Metal Model2 vertex library: " + describe(failure); return nil; }
        auto fs = [device newLibraryWithSource:@(fragment) options:nil error:&failure];
        if (!fs) { error = "Metal Model2 fragment library: " + describe(failure); return nil; }
        auto *desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = [vs newFunctionWithName:@"main0"];
        desc.fragmentFunction = [fs newFunctionWithName:@"main0"];
        desc.vertexDescriptor = vertex_layout(poly);
        desc.colorAttachments[0].pixelFormat = format;
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth16Unorm;
        auto state = [device newRenderPipelineStateWithDescriptor:desc error:&failure];
        if (!state) error = "Metal Model2 pipeline: " + describe(failure);
        return state;
    }
};

MetalModel2::MetalModel2() : impl_(std::make_unique<Impl>()) {}
MetalModel2::~MetalModel2() = default;
const std::string &MetalModel2::error() const { return impl_->error; }
void MetalModel2::set_polygon_scanlines(bool enabled) { impl_->frame.set_polygon_scanlines(enabled); }
void MetalModel2::shutdown() {
    auto &s = *impl_;
    s.polygon = s.back = s.front = s.quad = nil;
    s.depth_test = s.no_depth = nil;
    s.sampler = nil; s.device = nil; s.format = MTLPixelFormatInvalid;
}
bool MetalModel2::init(id<MTLDevice> device, MTLPixelFormat format) {
    shutdown(); auto &s = *impl_; s.error.clear();
    if (!device || (format != MTLPixelFormatBGRA8Unorm && format != MTLPixelFormatRGBA8Unorm)) {
        s.error = "Metal Model2: expected device and BGRA8/RGBA8 unorm target"; return false;
    }
    s.device = device; s.format = format;
    s.polygon = s.pipeline(k_vs_poly_msl, k_ps_poly_msl, true);
    s.back = s.pipeline(k_vs_quad_msl, k_ps_tiles_back_msl, false);
    s.front = s.pipeline(k_vs_quad_msl, k_ps_tiles_front_msl, false);
    s.quad = s.pipeline(k_vs_quad_msl, k_ps_quad_msl, false);
    if (!s.polygon || !s.back || !s.front || !s.quad) { shutdown(); return false; }
    auto *depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionLess;
    depth.depthWriteEnabled = YES;
    s.depth_test = [device newDepthStencilStateWithDescriptor:depth];
    depth.depthCompareFunction = MTLCompareFunctionAlways;
    depth.depthWriteEnabled = NO;
    s.no_depth = [device newDepthStencilStateWithDescriptor:depth];
    auto *sampler = [MTLSamplerDescriptor new];
    sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterNearest;
    sampler.mipFilter = MTLSamplerMipFilterNearest;
    sampler.sAddressMode = sampler.tAddressMode = sampler.rAddressMode = MTLSamplerAddressModeClampToEdge;
    s.sampler = [device newSamplerStateWithDescriptor:sampler];
    if (!s.depth_test || !s.no_depth || !s.sampler) {
        s.error = "Metal Model2: depth or sampler creation failed"; shutdown(); return false;
    }
    return true;
}

bool MetalModel2::encode(id<MTLCommandBuffer> command, id<MTLTexture> target, int w, int h,
                         const rt::Video &video, int scale) {
    auto &s = *impl_;
    if (!s.polygon || !command || !target || command.device != s.device || target.device != s.device ||
        !command.retainedReferences || command.status != MTLCommandBufferStatusNotEnqueued ||
        target.pixelFormat != s.format || target.textureType != MTLTextureType2D || target.sampleCount != 1 ||
        !(target.usage & MTLTextureUsageRenderTarget) || w <= 0 || h <= 0 || w != video.width() || h != rt::Video::H ||
        scale < 1 || scale > 4 || size_t(w) * size_t(scale) > target.width || size_t(h) * size_t(scale) > target.height) {
        s.error = "Metal Model2: invalid command, target, frame dimensions or scale"; return false;
    }
    const auto &mem = video.gpu_mem();
    if (!mem.palram || !mem.lumaram || !mem.colorxlat || !mem.tex0 || !mem.tex1) {
        s.error = "Metal Model2: missing frame palette/texture/luma/translation memory"; return false;
    }
    s.frame.build(video, w, h, scale);
    using namespace model2_resources;
    auto buffer = [&](size_t size) { return [s.device newBufferWithLength:size options:MTLResourceStorageModeShared]; };
    auto texram = buffer(texram_bytes), luma = buffer(luma_bytes), xlat = buffer(xlat_entries * 4);
    auto tilepix = buffer(tilepix_bytes), tiledata = buffer(tiledata_words * 4);
    auto edges = buffer(std::max(sizeof(Model2Frame::PolyEdge), s.frame.edges().size() * sizeof(Model2Frame::PolyEdge)));
    auto vertices = buffer(std::max(sizeof(Model2Frame::PolyVertex), s.frame.vertices().size() * sizeof(Model2Frame::PolyVertex)));
    const float fw = float(w), fh = float(h);
    const QuadVertex quad[6] = {{0,0,0,0},{fw,0,1,0},{0,fh,0,1},{fw,0,1,0},{fw,fh,1,1},{0,fh,0,1}};
    auto qbuf = [s.device newBufferWithBytes:quad length:sizeof quad options:MTLResourceStorageModeShared];
    auto *depth_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth16Unorm
        width:target.width height:target.height mipmapped:NO];
    depth_desc.storageMode = MTLStorageModePrivate; depth_desc.usage = MTLTextureUsageRenderTarget;
    auto depth = [s.device newTextureWithDescriptor:depth_desc];
    if (!texram || !luma || !xlat || !tilepix || !tiledata || !edges || !vertices || !qbuf || !depth) {
        s.error = "Metal Model2: frame resource allocation failed"; return false;
    }
    std::memcpy(texram.contents, mem.tex0, sheet_words * 4);
    std::memcpy(static_cast<uint8_t *>(texram.contents) + sheet_words * 4, mem.tex1, sheet_words * 4);
    std::memcpy(luma.contents, mem.lumaram, luma_bytes);
    pack_xlat(mem.colorxlat, s.gamma.data(), static_cast<uint32_t *>(xlat.contents));
    for (unsigned layer = 0; layer < 4; ++layer)
        pack_tile_rows(video, layer, 0, 512, static_cast<uint16_t *>(tilepix.contents) + layer * 512 * 512);
    pack_tile_data(video, scale, static_cast<uint32_t *>(tiledata.contents));
    std::memset(edges.contents, 0, edges.length);
    if (!s.frame.edges().empty()) std::memcpy(edges.contents, s.frame.edges().data(), s.frame.edges().size() * sizeof(Model2Frame::PolyEdge));
    if (!s.frame.vertices().empty()) std::memcpy(vertices.contents, s.frame.vertices().data(), s.frame.vertices().size() * sizeof(Model2Frame::PolyVertex));
    id<MTLTexture> front = nil;
    if (video.cpu_front()) {
        if (video.foreground_layer().size() != size_t(w) * size_t(h)) {
            s.error = "Metal Model2: incomplete CPU foreground"; return false;
        }
        auto *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:NSUInteger(w) height:NSUInteger(h) mipmapped:NO];
        desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageShaderRead;
        front = [s.device newTextureWithDescriptor:desc];
        if (!front) { s.error = "Metal Model2: foreground allocation failed"; return false; }
        [front replaceRegion:MTLRegionMake2D(0,0,NSUInteger(w),NSUInteger(h)) mipmapLevel:0
            withBytes:video.foreground_layer().data() bytesPerRow:size_t(w) * 4];
    }
    auto *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);
    pass.depthAttachment.texture = depth; pass.depthAttachment.clearDepth = 1;
    pass.depthAttachment.loadAction = MTLLoadActionClear; pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    auto encoder = [command renderCommandEncoderWithDescriptor:pass];
    if (!encoder) { s.error = "Metal Model2: render encoder creation failed"; return false; }
    encoder.label = @"Sega Rally Model 2";
    [encoder setCullMode:MTLCullModeNone];
    [encoder setDepthClipMode:MTLDepthClipModeClamp];
    [encoder setViewport:MTLViewport{0,0,double(w * scale),double(h * scale),0,1}];
    const float screen[4] = {fw,fh,0,0};
    [encoder setVertexBytes:screen length:sizeof screen atIndex:0];
    const MTLScissorRect full{0,0,NSUInteger(w * scale),NSUInteger(h * scale)};
    auto draw_quad = [&](id<MTLRenderPipelineState> pipeline) {
        [encoder setRenderPipelineState:pipeline]; [encoder setDepthStencilState:s.no_depth];
        [encoder setScissorRect:full]; [encoder setVertexBuffer:qbuf offset:0 atIndex:1];
        if (pipeline == s.quad) {
            [encoder setFragmentTexture:front atIndex:0]; [encoder setFragmentSamplerState:s.sampler atIndex:0];
        } else {
            [encoder setFragmentBuffer:tilepix offset:0 atIndex:0]; [encoder setFragmentBuffer:tiledata offset:0 atIndex:1];
        }
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    };
    draw_quad(s.back);
    if (!s.frame.vertices().empty()) {
        [encoder setRenderPipelineState:s.polygon]; [encoder setDepthStencilState:s.depth_test];
        [encoder setVertexBuffer:vertices offset:0 atIndex:1];
        [encoder setFragmentBuffer:texram offset:0 atIndex:0]; [encoder setFragmentBuffer:luma offset:0 atIndex:1];
        [encoder setFragmentBuffer:xlat offset:0 atIndex:2]; [encoder setFragmentBuffer:edges offset:0 atIndex:3];
        for (const auto &batch : s.frame.batches()) {
            if (!batch.count) continue;
            const auto &clip = batch.clip;
            [encoder setScissorRect:MTLScissorRect{NSUInteger(clip.x * scale),NSUInteger(clip.y * scale),
                NSUInteger(clip.w * scale),NSUInteger(clip.h * scale)}];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:batch.first vertexCount:batch.count];
        }
    }
    draw_quad(video.cpu_front() ? s.quad : s.front);
    [encoder endEncoding]; s.error.clear(); return true;
}
} // namespace app::native
