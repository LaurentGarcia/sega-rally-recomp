// Direct Metal Model 2 submission; host owns command buffers and presentation.
#pragma once
#import <Metal/Metal.h>
#include <memory>
#include <string>

namespace rt { class Video; }
namespace app::native {
class MetalModel2 {
public:
    MetalModel2();
    ~MetalModel2();
    MetalModel2(const MetalModel2 &) = delete;
    MetalModel2 &operator=(const MetalModel2 &) = delete;
    bool init(id<MTLDevice>, MTLPixelFormat);
    void shutdown();
    const std::string &error() const;
    void set_polygon_scanlines(bool enabled);
    // Mirrors GpuRenderer's back tiles / ordered polygons / front tiles.
    // Uses the shared Model2Frame and generated production MSL without edits.
    // Dimensions must match Video; scale is 1..4. Target is single-sample 2D,
    // renderable, and at least width*scale by height*scale on init's device.
    // Requires a fresh retained-reference command buffer on that device.
    // No commit, wait, drawable or SDL calls. Snapshot resources are immutable
    // and retained by the command buffer, including across another encode.
    // This correctness implementation allocates full snapshots each frame;
    // persistent upload rings and performance qualification are separate work.
    bool encode(id<MTLCommandBuffer>, id<MTLTexture> target, int width, int height,
                const rt::Video &, int scale = 1);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace app::native
