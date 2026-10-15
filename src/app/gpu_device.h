#pragma once
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>

namespace app {
// Rally supports Metal on Apple and Vulkan on Windows/Linux.
inline constexpr const char *default_gpu_driver() {
#ifdef __APPLE__
    return "metal";
#else
    return "vulkan";
#endif
}
inline bool supported_gpu_driver(const char *driver) {
    return driver && SDL_strcasecmp(driver,default_gpu_driver())==0;
}
// Null preserves SDL automatic selection for existing non-Rally callers.
inline SDL_GPUDevice *create_gpu_device(const char *preferred_driver=nullptr) {
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) return nullptr;
    if(preferred_driver && !SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,preferred_driver)) {
        SDL_DestroyProperties(props);return nullptr;
    }
    // Game shaders use DXIL; pinned ImGui supplies DXBC on D3D12.
    // Same shaders on every platform; no shader clip distances, indirect
    // draws or anisotropic samplers are used by the game or ImGui.
    const bool configured =
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
    // Keep depth clamping: current pipelines (including pinned ImGui) set
    // enable_depth_clip=false. Disabling the feature alone is not valid.
    SDL_GPUDevice *device = configured ? SDL_CreateGPUDeviceWithProperties(props) : nullptr;
    SDL_DestroyProperties(props);
    return device;
}
} // namespace app
