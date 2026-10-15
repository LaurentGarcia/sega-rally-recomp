#pragma once

#include "app/gpu/crt_settings.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_rect.h>
#include <string>

namespace app {
// A separate presentation pass after complete native image composition and
// before touch/UI. It uses the same generated HLSL on every SDL GPU backend.
class CrtRenderer {
public:
    bool init(SDL_GPUDevice *device, SDL_GPUTextureFormat format);
    void shutdown();
    const std::string &error() const { return error_; }
    // Disabled: encode nothing and return true; the host uses its unchanged
    // presentation blit. Enabled: clear destination to black, then draw viewport.
    // source_size is the texture's full rendered extent. native_size controls
    // beam count and chromatic shift units when the source is rendered larger.
    // CRT sampling is always linear; the host's filter option applies to its
    // original presentation path. Caller owns source/destination and submission.
    bool render(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *source, SDL_GPUTexture *destination,
                const SDL_Rect &viewport, const CrtSettings &settings,
                SDL_Point source_size = {496, 384}, SDL_Point native_size = {496, 384});

private:
    SDL_GPUDevice *device_ = nullptr;
    SDL_GPUGraphicsPipeline *pipeline_ = nullptr;
    SDL_GPUSampler *sampler_ = nullptr;
    std::string error_;
};
} // namespace app
