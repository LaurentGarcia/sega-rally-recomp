// Internal adapter for the pinned ImGui Vulkan renderer. Panel context is current.
#pragma once
#include "app/native/vulkan/vulkan_presenter.h"
struct ImDrawData;
namespace app::display_vulkan {
using Context = native::VulkanPresenter::UIContext;
bool valid(const Context &,std::string &error);
bool same_device(const Context &,const Context &);
class Backend {
public:
    bool init(const Context &,std::string &error);
    bool new_frame(const Context &,std::string &error);
    bool render(ImDrawData *,VkCommandBuffer,std::string &error);
    void shutdown(); // Device remains alive; drains UI resources before release.
private:
    Context context_{};
    bool initialized_=false,failed_=false;
};
}
