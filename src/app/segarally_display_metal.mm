#include "app/segarally_display_metal.h"
#include "backends/imgui_impl_metal.h"
#import <Metal/Metal.h>

namespace app::display_metal {
bool init(void *device) {
    return device && ImGui_ImplMetal_Init((__bridge id<MTLDevice>)device);
}
void shutdown() { ImGui_ImplMetal_Shutdown(); }
bool new_frame(void *device, void *render_pass_descriptor) {
    auto pass = (__bridge MTLRenderPassDescriptor *)render_pass_descriptor;
    auto target = pass.colorAttachments[0].texture;
    if (!device || !pass || !target || target.device != (__bridge id<MTLDevice>)device ||
        target.pixelFormat != MTLPixelFormatBGRA8Unorm || target.sampleCount != 1 ||
        target.textureType != MTLTextureType2D || !(target.usage & MTLTextureUsageRenderTarget)) return false;
    ImGui_ImplMetal_NewFrame(pass);
    return true;
}
bool render(ImDrawData *draw, void *device, void *command_buffer, void *render_encoder) {
    auto command = (__bridge id<MTLCommandBuffer>)command_buffer;
    auto encoder = (__bridge id<MTLRenderCommandEncoder>)render_encoder;
    if (!draw || !device || !command || !encoder || command.device != (__bridge id<MTLDevice>)device ||
        encoder.device != (__bridge id<MTLDevice>)device || !command.retainedReferences ||
        command.status != MTLCommandBufferStatusNotEnqueued) return false;
    ImGui_ImplMetal_RenderDrawData(draw, command, encoder);
    return true;
}
}
