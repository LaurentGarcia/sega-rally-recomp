// Native presentation only. Host owns command submission, drawable and pacing.
#pragma once
#import <Metal/Metal.h>
#include "app/gpu/crt_settings.h"
#include <cstdint>
#include <memory>
#include <string>

namespace app::native {
struct PixelViewport {uint32_t x=0,y=0,w=0,h=0;};
struct PixelSize {uint32_t w=0,h=0;};
class MetalCrt {
public:
    MetalCrt();
    ~MetalCrt();
    MetalCrt(const MetalCrt &)=delete;
    MetalCrt &operator=(const MetalCrt &)=delete;
    bool init(id<MTLDevice>,MTLPixelFormat);
    // Disabled is a no-op, matching CrtRenderer. The host performs its ordinary
    // presentation when disabled. Enabled clears target black and draws viewport.
    // Source/target must belong to init's device, be distinct single-sample 2D
    // textures; target format must match init. No commit/wait/present occurs here.
    bool encode(id<MTLCommandBuffer>,id<MTLTexture> source,id<MTLTexture> target,
                PixelViewport,const CrtSettings &,PixelSize native_size);
    void shutdown();
    const std::string &error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace app::native
