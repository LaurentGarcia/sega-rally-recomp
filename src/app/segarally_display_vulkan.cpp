#include "app/segarally_display_vulkan.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include <algorithm>

namespace app::display_vulkan {
namespace {
void check_result(VkResult result) {
    // Stop the pinned backend immediately on allocation/device errors; it must
    // not continue into a memory copy using a failed mapping or allocation.
    if (result<0) throw native::VulkanError(result,"Display panel");
}
uint32_t buffer_count(const Context &context) {
    // Swapchain image count can be smaller than the presenter's command ring.
    // Its slot fence is waited before render(), where ImGui writes vertices.
    return std::max(context.image_count,native::VulkanModel2::frames_in_flight);
}
}
bool valid(const Context &context,std::string &error) {
    if (!context.instance || !context.physical_device || !context.device || !context.queue ||
        !context.render_pass || context.min_image_count<2 || context.image_count<context.min_image_count) {
        error="Display panel requires a complete Vulkan presentation context.";return false;
    }
    return true;
}
bool same_device(const Context &a,const Context &b) {
    return a.instance==b.instance && a.physical_device==b.physical_device &&
        a.device==b.device && a.queue==b.queue && a.queue_family==b.queue_family;
}
bool Backend::init(const Context &context,std::string &error) {
    if (!valid(context,error)) return false;
    context_=context;failed_=false;
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion=VK_API_VERSION_1_1;
    info.Instance=context.instance;info.PhysicalDevice=context.physical_device;
    info.Device=context.device;info.QueueFamily=context.queue_family;info.Queue=context.queue;
    info.DescriptorPoolSize=32;
    info.MinImageCount=context.min_image_count;info.ImageCount=buffer_count(context);
    info.PipelineInfoMain.RenderPass=context.render_pass;
    info.PipelineInfoMain.MSAASamples=VK_SAMPLE_COUNT_1_BIT;
    info.CheckVkResultFn=check_result;
    try {
        initialized_=ImGui_ImplVulkan_Init(&info);
        if (!initialized_) error="Cannot initialize Vulkan display controls.";
    } catch (const std::exception &exception) {
        error=exception.what();
        // Init registers backend data before creating device objects. Release
        // that partial state too, while this panel's context is current.
        initialized_=ImGui::GetIO().BackendRendererUserData!=nullptr;
        shutdown();failed_=true;return false;
    }
    return initialized_;
}
bool Backend::new_frame(const Context &context,std::string &error) {
    if (!initialized_ || failed_) {error="Vulkan display controls are unavailable.";return false;}
    if (!valid(context,error)) return false;
    if (!same_device(context_,context)) {error="Display panel Vulkan device changed.";return false;}
    try {
        if (context.render_pass!=context_.render_pass || context.min_image_count!=context_.min_image_count ||
            buffer_count(context)!=buffer_count(context_)) {
            check_result(vkDeviceWaitIdle(context_.device));
            // SetMinImageCount alone leaves the backend's ImageCount unchanged.
            // Recreate only on incompatible pass/count changes, never per frame.
            ImGui_ImplVulkan_Shutdown();initialized_=false;
            if (!init(context,error)) return false;
        }
        context_=context;
        ImGui_ImplVulkan_NewFrame();return true;
    } catch (const std::exception &exception) {error=exception.what();failed_=true;return false;}
}
bool Backend::render(ImDrawData *draw,VkCommandBuffer command,std::string &error) {
    if (!initialized_ || failed_) {error="Vulkan display controls are unavailable.";return false;}
    if (!command) {error="Display panel requires an active Vulkan command buffer.";return false;}
    try {ImGui_ImplVulkan_RenderDrawData(draw,command);return true;}
    catch (const std::exception &exception) {error=exception.what();failed_=true;return false;}
}
void Backend::shutdown() {
    if (initialized_) {
        // The host normally drained already. This also makes panel reinit safe
        // while the previous UI frame still refers to its vertex/font buffers.
        vkDeviceWaitIdle(context_.device);
        ImGui_ImplVulkan_Shutdown();
    }
    initialized_=failed_=false;context_={};
}
}
