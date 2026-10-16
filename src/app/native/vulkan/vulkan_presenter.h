#pragma once
#include "app/native/vulkan/model2_vulkan.h"
#include "app/gpu/crt_settings.h"
#include <filesystem>
struct SDL_Window;
namespace app::native {
// SDL creates only the OS window/surface. All rendering, resource ownership,
// frame pacing and presentation below use Vulkan directly, never SDL_GPU.
class VulkanPresenter {
public:
    enum class Outcome {presented,paused,recreate,device_lost,error};
    struct Statistics {uint64_t presentations=0,swapchain_creations=0,resize_idle_waits=0,pipeline_creations=0;};
    struct UIContext {
        VkInstance instance{};VkPhysicalDevice physical_device{};VkDevice device{};VkQueue queue{};
        uint32_t queue_family=0;VkRenderPass render_pass{};uint32_t min_image_count=0,image_count=0;
        uint64_t swapchain_generation=0;
    };
    VulkanPresenter();~VulkanPresenter();
    VulkanPresenter(const VulkanPresenter &)=delete;
    VulkanPresenter &operator=(const VulkanPresenter &)=delete;
    bool init(SDL_Window *,bool require_software=false,bool validation=false);
    Outcome present(const rt::Video &,int scale,bool polygon_scanlines,const CrtSettings &,
                    bool update_game=true,bool has_game=true,
                    const std::function<void(VkCommandBuffer)> &overlay={},
                    bool linear_filter=true,bool integer_scale=false);
    UIContext ui_context() const;
    void drain();
    // Raw game image at its current internal render extent, before CRT and UI.
    bool capture(const std::filesystem::path &);
    const std::string &error() const;
    Statistics statistics() const;
    const VulkanModel2 &renderer() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
