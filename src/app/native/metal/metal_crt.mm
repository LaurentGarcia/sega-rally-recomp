#include "app/native/metal/metal_crt.h"
#include "app/gpu/crt_shaders_gen.h"
#include <algorithm>
#include <cmath>

namespace app::native {
namespace {
float strength(float value,float maximum){return std::isfinite(value)?std::clamp(value,0.f,maximum):0.f;}
std::string message(NSError *error){return error?std::string(error.localizedDescription.UTF8String):"unknown Metal error";}
}
struct MetalCrt::Impl {
    id<MTLDevice> device=nil;
    id<MTLRenderPipelineState> pipeline=nil;
    id<MTLSamplerState> sampler=nil;
    MTLPixelFormat format=MTLPixelFormatInvalid;
    std::string error;
};
MetalCrt::MetalCrt():impl_(std::make_unique<Impl>()){}
MetalCrt::~MetalCrt()=default;
void MetalCrt::shutdown(){auto &s=*impl_;s.pipeline=nil;s.sampler=nil;s.device=nil;s.format=MTLPixelFormatInvalid;}
const std::string &MetalCrt::error()const{return impl_->error;}
bool MetalCrt::init(id<MTLDevice> device,MTLPixelFormat format){
    shutdown();auto &s=*impl_;s.error.clear();
    if(!device||format==MTLPixelFormatInvalid){s.error="Metal CRT: missing device or format";return false;}
    NSError *error=nil;
    // Reuse the generated MSL byte-for-byte and the same default Metal compiler
    // options as the pinned SDL Metal backend. No platform shader fork.
    auto vertex_library=[device newLibraryWithSource:@(k_vs_crt_msl) options:nil error:&error];
    if(!vertex_library){s.error="Metal CRT vertex library: "+message(error);return false;}
    auto fragment_library=[device newLibraryWithSource:@(k_ps_crt_msl) options:nil error:&error];
    if(!fragment_library){s.error="Metal CRT fragment library: "+message(error);return false;}
    MTLRenderPipelineDescriptor *pipeline=[MTLRenderPipelineDescriptor new];
    pipeline.label=@"Sega Rally CRT";
    pipeline.vertexFunction=[vertex_library newFunctionWithName:@"main0"];
    pipeline.fragmentFunction=[fragment_library newFunctionWithName:@"main0"];
    pipeline.colorAttachments[0].pixelFormat=format;
    s.pipeline=[device newRenderPipelineStateWithDescriptor:pipeline error:&error];
    if(!s.pipeline){s.error="Metal CRT pipeline: "+message(error);return false;}
    MTLSamplerDescriptor *sampler=[MTLSamplerDescriptor new];
    sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;
    sampler.mipFilter=MTLSamplerMipFilterNearest;
    sampler.sAddressMode=sampler.tAddressMode=sampler.rAddressMode=MTLSamplerAddressModeClampToEdge;
    s.sampler=[device newSamplerStateWithDescriptor:sampler];
    if(!s.sampler){s.error="Metal CRT sampler creation failed";shutdown();return false;}
    s.device=device;s.format=format;return true;
}
bool MetalCrt::encode(id<MTLCommandBuffer> command,id<MTLTexture> source,id<MTLTexture> target,
                       PixelViewport viewport,const CrtSettings &settings,PixelSize native){
    if(!settings.enabled)return true;
    auto &s=*impl_;
    if(!command||!s.pipeline||!source||!target||source==target||!viewport.w||!viewport.h||!native.w||!native.h||
       command.device!=s.device||source.device!=s.device||target.device!=s.device||target.pixelFormat!=s.format||
       source.textureType!=MTLTextureType2D||target.textureType!=MTLTextureType2D||source.sampleCount!=1||target.sampleCount!=1||
       viewport.x>target.width||viewport.y>target.height||viewport.w>target.width-viewport.x||viewport.h>target.height-viewport.y){
        s.error="Metal CRT: invalid device, resource, viewport or native size";return false;
    }
    struct Uniforms{float viewport[4],sizes[4],strength[4],extra[4];};
    static_assert(sizeof(Uniforms)==64);
    const Uniforms uniforms{{float(viewport.x),float(viewport.y),float(viewport.w),float(viewport.h)},
        {float(source.width),float(source.height),float(native.w),float(native.h)},
        {strength(settings.scanlines,1),strength(settings.mask,1),strength(settings.glow,.5f),strength(settings.curvature,.15f)},
        {strength(settings.chromatic,1),0,0,0}};
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=target;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
    auto encoder=[command renderCommandEncoderWithDescriptor:pass];
    if(!encoder){s.error="Metal CRT render encoder creation failed";return false;}
    encoder.label=@"Sega Rally CRT";
    [encoder setRenderPipelineState:s.pipeline];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setViewport:MTLViewport{double(viewport.x),double(viewport.y),double(viewport.w),double(viewport.h),0,1}];
    [encoder setScissorRect:MTLScissorRect{viewport.x,viewport.y,viewport.w,viewport.h}];
    [encoder setFragmentTexture:source atIndex:0];[encoder setFragmentSamplerState:s.sampler atIndex:0];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];return true;
}
} // namespace app::native
