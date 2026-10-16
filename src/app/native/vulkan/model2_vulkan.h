// Direct Vulkan Model 2 rendering. Presentation/window ownership is external.
#pragma once
#include "app/gpu/model2_frame.h"
#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace rt {class Video;}
namespace app::native {
class VulkanError : public std::runtime_error {
public:
    VulkanError(VkResult result,const char *operation):std::runtime_error(std::string(operation)+": Vulkan result "+std::to_string(result)),result(result){}
    VkResult result;
};
class VulkanModel2 {
public:
    static constexpr uint32_t frames_in_flight=3;
    struct Config {
        bool require_software=false,validation=false;
        std::vector<const char *> instance_extensions;
        // Optional window surface, created before physical-device selection.
        // This renderer owns the returned surface and destroys it with instance.
        std::function<VkSurfaceKHR(VkInstance)> create_surface;
    };
    struct EncodedFrame {
        VkCommandBuffer command{};
        VkImage image{};VkImageView view{};VkExtent2D extent{},native_extent{};
        uint32_t slot=0;
    };
    struct Readback {std::vector<uint32_t> pixels;VkExtent2D extent{},native_extent{};};
    struct Statistics {uint64_t submissions=0,pipeline_creations=0,buffer_allocations=0,image_allocations=0,slot_waits=0;};
    VulkanModel2();~VulkanModel2();
    VulkanModel2(const VulkanModel2 &)=delete;
    VulkanModel2 &operator=(const VulkanModel2 &)=delete;
    bool init(bool require_software=false,bool validation=false);
    bool init(const Config &);
    // Single render thread. Wait before reusing this slot's acquire semaphore.
    uint32_t next_slot() const;
    void wait_next();
    // The returned command is still recording and image is shader-read-only.
    // Callers may append presentation commands, then submit exactly once.
    // Frame snapshots remain immutable until that slot's submission completes.
    EncodedFrame encode(const rt::Video &,int width,int height,int scale=1,bool scanlines=false);
    // Begin a presentation-only command using the last completed/submitted game image.
    EncodedFrame begin_cached(bool has_game=true);
    void submit(VkSemaphore acquire=VK_NULL_HANDLE,VkSemaphore rendered=VK_NULL_HANDLE,
                VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    void drain();
    // Diagnostic adapter over the same encoder: submit, wait, read BGRA8.
    std::vector<uint32_t> render(const rt::Video &,int width,int height,int scale=1,bool scanlines=false);
    // Synchronous diagnostic capture of the cached game; leaves it sampleable.
    Readback readback();
    const Model2Frame &frame() const;
    const std::string &device_name() const;
    uint32_t validation_errors() const;
    Statistics statistics() const;
    VkInstance instance() const;VkPhysicalDevice physical_device() const;
    VkDevice device() const;VkQueue queue() const;VkSurfaceKHR surface() const;
    uint32_t queue_family() const;
private:
    Readback readback_encoded(const EncodedFrame &);
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
