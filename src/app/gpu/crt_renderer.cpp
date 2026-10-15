#include "app/gpu/crt_renderer.h"
#include "app/gpu/crt_shaders_gen.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {
namespace {
float strength(float value, float maximum) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, maximum) : 0.0f;
}
SDL_GPUShader *shader(SDL_GPUDevice *device, const unsigned char *spv, size_t spv_size,
                      const unsigned char *dxil, size_t dxil_size, const char *msl,
                      const char *entry, SDL_GPUShaderStage stage) {
    SDL_GPUShaderCreateInfo info{};
    info.stage = stage;
    info.num_samplers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1 : 0;
    info.num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1 : 0;
    const auto formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV; info.code = spv; info.code_size = spv_size; info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL; info.code = dxil; info.code_size = dxil_size; info.entrypoint = entry;
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL; info.code = reinterpret_cast<const Uint8 *>(msl);
        info.code_size = std::strlen(msl); info.entrypoint = "main0";
    } else {
        SDL_SetError("CRT renderer: no supported shader format"); return nullptr;
    }
    return SDL_CreateGPUShader(device, &info);
}
} // namespace

bool CrtRenderer::init(SDL_GPUDevice *device, SDL_GPUTextureFormat format) {
    shutdown(); error_.clear();
    if (!device) { error_ = "CRT renderer: missing GPU device"; return false; }
    device_ = device;
    auto *vs = shader(device, k_vs_crt_spv, sizeof k_vs_crt_spv, k_vs_crt_dxil, sizeof k_vs_crt_dxil,
                      k_vs_crt_msl, "vs_crt", SDL_GPU_SHADERSTAGE_VERTEX);
    auto *ps = shader(device, k_ps_crt_spv, sizeof k_ps_crt_spv, k_ps_crt_dxil, sizeof k_ps_crt_dxil,
                      k_ps_crt_msl, "ps_crt", SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (vs && ps) {
        SDL_GPUColorTargetDescription color{}; color.format = format;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vs; info.fragment_shader = ps;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.color_target_descriptions = &color; info.target_info.num_color_targets = 1;
        pipeline_ = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (vs) SDL_ReleaseGPUShader(device, vs);
    if (ps) SDL_ReleaseGPUShader(device, ps);
    if (!pipeline_) { error_ = std::string("CRT pipeline: ") + SDL_GetError(); shutdown(); return false; }
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(device, &sampler);
    if (!sampler_) { error_ = std::string("CRT sampler: ") + SDL_GetError(); shutdown(); return false; }
    return true;
}

void CrtRenderer::shutdown() {
    if (device_) {
        if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
        if (sampler_) SDL_ReleaseGPUSampler(device_, sampler_);
    }
    pipeline_ = nullptr; sampler_ = nullptr; device_ = nullptr;
}

bool CrtRenderer::render(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *source, SDL_GPUTexture *destination,
                         const SDL_Rect &viewport, const CrtSettings &settings,
                         SDL_Point source_size, SDL_Point native_size) {
    if (!settings.enabled) return true;
    if (!pipeline_ || !sampler_ || !cmd || !source || !destination || source == destination ||
        viewport.x < 0 || viewport.y < 0 || viewport.w <= 0 || viewport.h <= 0 ||
        source_size.x <= 0 || source_size.y <= 0 || native_size.x <= 0 || native_size.y <= 0) {
        error_ = "CRT render: invalid resource, viewport or source size"; return false;
    }
    struct Uniforms { float viewport[4], sizes[4], strength[4], extra[4]; };
    static_assert(sizeof(Uniforms) == 64);
    const Uniforms uniforms{{float(viewport.x), float(viewport.y), float(viewport.w), float(viewport.h)},
                            {float(source_size.x), float(source_size.y), float(native_size.x), float(native_size.y)},
                            {strength(settings.scanlines, 1), strength(settings.mask, 1),
                             strength(settings.glow, 0.5f), strength(settings.curvature, 0.15f)},
                            {strength(settings.chromatic, 1), 0, 0, 0}};
    SDL_GPUColorTargetInfo color{};
    color.texture = destination; color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
    color.clear_color = {0, 0, 0, 1};
    auto *pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
    if (!pass) { error_ = std::string("CRT pass: ") + SDL_GetError(); return false; }
    const SDL_GPUViewport target{float(viewport.x), float(viewport.y), float(viewport.w), float(viewport.h), 0, 1};
    SDL_SetGPUViewport(pass, &target); SDL_SetGPUScissor(pass, &viewport);
    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    const SDL_GPUTextureSamplerBinding texture{source, sampler_};
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof uniforms);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}
} // namespace app
