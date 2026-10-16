#include "app/native/vulkan/vulkan_presenter.h"
#include "app/gpu/crt_shaders_gen.h"
#include "runtime/video.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <fstream>
#include <cstdio>

namespace app::native {
namespace {
void check(VkResult result,const char *what){if(result!=VK_SUCCESS)throw VulkanError(result,what);}
float strength(float value,float maximum){return std::isfinite(value)?std::clamp(value,0.f,maximum):0.f;}
struct Uniforms {float viewport[4],sizes[4],strength[4],extra[4];};
static_assert(sizeof(Uniforms)==64);
}
struct VulkanPresenter::Impl {
    VulkanModel2 renderer;SDL_Window *window=nullptr;std::string error;bool lost=false,failed=false,dirty=true;
    VkDevice device{};VkSwapchainKHR swapchain{};VkExtent2D extent{};uint32_t minimum_images=0;
    std::vector<VkImage> images;std::vector<VkImageView> views;std::vector<VkFramebuffer> framebuffers;
    // Present wait semaphores are indexed by acquired image, not frame slot.
    std::vector<VkSemaphore> finished;
    struct Slot {VkSemaphore acquired{};VkBuffer uniform{};VkDeviceMemory memory{};std::array<VkDescriptorSet,4> sets{};};
    std::array<Slot,VulkanModel2::frames_in_flight> slots;
    std::array<VkDescriptorSetLayout,4> layouts{};VkDescriptorPool descriptors{};
    std::array<VkSampler,2> samplers{};VkRenderPass pass{},load_pass{};VkPipelineLayout pipeline_layout{};VkPipeline pipeline{};
    VkShaderModule vertex{},fragment{};Statistics stats;
    ~Impl(){
        if(!device)return;
        vkDeviceWaitIdle(device);destroy_swapchain();
        if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
        if(pipeline_layout)vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
        if(pass)vkDestroyRenderPass(device,pass,nullptr);
        if(load_pass)vkDestroyRenderPass(device,load_pass,nullptr);
        if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
        for(auto layout:layouts)if(layout)vkDestroyDescriptorSetLayout(device,layout,nullptr);
        for(auto sampler:samplers)if(sampler)vkDestroySampler(device,sampler,nullptr);
        if(vertex)vkDestroyShaderModule(device,vertex,nullptr);
        if(fragment)vkDestroyShaderModule(device,fragment,nullptr);
        for(auto &slot:slots){
            if(slot.acquired)vkDestroySemaphore(device,slot.acquired,nullptr);
            if(slot.uniform)vkDestroyBuffer(device,slot.uniform,nullptr);
            if(slot.memory)vkFreeMemory(device,slot.memory,nullptr);
        }
    }
    void destroy_swapchain(){
        for(auto framebuffer:framebuffers)if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
        for(auto view:views)if(view)vkDestroyImageView(device,view,nullptr);
        for(auto semaphore:finished)if(semaphore)vkDestroySemaphore(device,semaphore,nullptr);
        framebuffers.clear();views.clear();finished.clear();images.clear();
        if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);swapchain=VK_NULL_HANDLE;
    }
    uint32_t memory_type(uint32_t bits){
        VkPhysicalDeviceMemoryProperties properties{};vkGetPhysicalDeviceMemoryProperties(renderer.physical_device(),&properties);
        for(uint32_t i=0;i<properties.memoryTypeCount;++i)if((bits&(1u<<i))&&
            (properties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))return i;
        throw std::runtime_error("No coherent presentation uniform memory");
    }
    VkShaderModule module(const unsigned char *bytes,size_t length){
        std::vector<uint32_t> words(length/4);std::memcpy(words.data(),bytes,length);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=length;info.pCode=words.data();
        VkShaderModule result{};check(vkCreateShaderModule(device,&info,nullptr,&result),"presentation shader");return result;
    }
    void init_resources(){
        VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(renderer.physical_device(),VK_FORMAT_B8G8R8A8_UNORM,&format);
        constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((format.optimalTilingFeatures&required)!=required)throw std::runtime_error("BGRA8 native blit/filter support is unavailable");
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;check(vkCreateSampler(device,&si,nullptr,&samplers[1]),"linear presentation sampler");
        si.magFilter=si.minFilter=VK_FILTER_NEAREST;check(vkCreateSampler(device,&si,nullptr,&samplers[0]),"nearest presentation sampler");
        for(unsigned i=0;i<4;++i){
            VkDescriptorSetLayoutBinding binding{0,i==2?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
            VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=i<2?0:1;li.pBindings=&binding;
            check(vkCreateDescriptorSetLayout(device,&li,nullptr,&layouts[i]),"presentation layout");
        }
        const VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=12;pi.poolSizeCount=2;pi.pPoolSizes=sizes;
        check(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors),"presentation descriptors");
        for(auto &slot:slots){
            VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};check(vkCreateSemaphore(device,&sem,nullptr,&slot.acquired),"acquire semaphore");
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=sizeof(Uniforms);buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            check(vkCreateBuffer(device,&buffer,nullptr,&slot.uniform),"presentation uniform");VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,slot.uniform,&req);
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memory_type(req.memoryTypeBits);
            check(vkAllocateMemory(device,&alloc,nullptr,&slot.memory),"presentation memory");check(vkBindBufferMemory(device,slot.uniform,slot.memory,0),"presentation memory binding");
            VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sets.descriptorPool=descriptors;sets.descriptorSetCount=4;sets.pSetLayouts=layouts.data();
            check(vkAllocateDescriptorSets(device,&sets,slot.sets.data()),"presentation sets");
            VkDescriptorBufferInfo data{slot.uniform,0,sizeof(Uniforms)};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=slot.sets[3];write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&data;
            vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        }
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=4;pli.pSetLayouts=layouts.data();
        check(vkCreatePipelineLayout(device,&pli,nullptr,&pipeline_layout),"presentation pipeline layout");
        VkAttachmentDescription attachment{};attachment.format=VK_FORMAT_B8G8R8A8_UNORM;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;
        VkRenderPassCreateInfo pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pass_info.attachmentCount=1;pass_info.pAttachments=&attachment;pass_info.subpassCount=1;pass_info.pSubpasses=&sub;
        check(vkCreateRenderPass(device,&pass_info,nullptr,&pass),"presentation pass");
        // Load/store operations do not affect render-pass compatibility; the
        // same framebuffer/UI pipeline can overlay a completed native blit.
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
        check(vkCreateRenderPass(device,&pass_info,nullptr,&load_pass),"presentation load pass");
        vertex=module(k_vs_crt_spv,sizeof k_vs_crt_spv);fragment=module(k_ps_crt_spv,sizeof k_ps_crt_spv);
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};for(auto &stage:stages){stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fragment;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&blend;
        const VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=states;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&ds;gp.layout=pipeline_layout;gp.renderPass=pass;
        check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline),"presentation pipeline");++stats.pipeline_creations;
    }
    bool resize(uint32_t width,uint32_t height){
        VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer.physical_device(),renderer.surface(),&caps),"surface capabilities");
        auto size=caps.currentExtent.width==std::numeric_limits<uint32_t>::max()?VkExtent2D{std::clamp(width,caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(height,caps.minImageExtent.height,caps.maxImageExtent.height)}:caps.currentExtent;
        if(!size.width||!size.height)return false;
        uint32_t count=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physical_device(),renderer.surface(),&count,nullptr),"surface format count");std::vector<VkSurfaceFormatKHR> formats(count);
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physical_device(),renderer.surface(),&count,formats.data()),"surface formats");
        const auto suitable=std::find_if(formats.begin(),formats.end(),[](const auto &f){return f.format==VK_FORMAT_B8G8R8A8_UNORM&&f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;});
        if(suitable==formats.end())throw std::runtime_error("Surface requires an unsupported color format (BGRA8 UNORM required)");
        constexpr VkImageUsageFlags required_usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if((caps.supportedUsageFlags&required_usage)!=required_usage)throw std::runtime_error("Surface requires color attachment and native blit destination support");
        if(swapchain){renderer.drain();check(vkDeviceWaitIdle(device),"swapchain resize idle");++stats.resize_idle_waits;destroy_swapchain();}
        uint32_t image_count=std::max(caps.minImageCount,3u);if(caps.maxImageCount)image_count=std::min(image_count,caps.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if(!(caps.supportedCompositeAlpha&alpha))for(auto option:{VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})if(caps.supportedCompositeAlpha&option){alpha=option;break;}
        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};ci.surface=renderer.surface();ci.minImageCount=image_count;ci.imageFormat=suitable->format;ci.imageColorSpace=suitable->colorSpace;ci.imageExtent=size;ci.imageArrayLayers=1;ci.imageUsage=required_usage;ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.preTransform=caps.currentTransform;ci.compositeAlpha=alpha;ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;ci.clipped=VK_TRUE;
        check(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain),"swapchain");extent=size;
        check(vkGetSwapchainImagesKHR(device,swapchain,&count,nullptr),"swapchain image count");images.resize(count);views.resize(count);framebuffers.resize(count);finished.resize(count);
        check(vkGetSwapchainImagesKHR(device,swapchain,&count,images.data()),"swapchain images");
        for(uint32_t i=0;i<count;++i){
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=images[i];view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=suitable->format;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(vkCreateImageView(device,&view,nullptr,&views[i]),"swapchain view");
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=pass;fb.attachmentCount=1;fb.pAttachments=&views[i];fb.width=size.width;fb.height=size.height;fb.layers=1;check(vkCreateFramebuffer(device,&fb,nullptr,&framebuffers[i]),"swapchain framebuffer");
            VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};check(vkCreateSemaphore(device,&sem,nullptr,&finished[i]),"presentation semaphore");
        }
        dirty=false;minimum_images=image_count;++stats.swapchain_creations;return true;
    }
    Outcome draw(const rt::Video &video,int scale,bool scanlines,const CrtSettings &settings,bool update_game,bool has_game,const std::function<void(VkCommandBuffer)> &overlay,bool linear_filter,bool integer_scale){
        if(lost)return Outcome::device_lost;if(failed)return Outcome::error;
        int width=0,height=0;if(!SDL_GetWindowSizeInPixels(window,&width,&height))throw std::runtime_error(SDL_GetError());
        const auto offscreen=[&]{if(update_game&&has_game){renderer.encode(video,video.width(),rt::Video::H,scale,scanlines);renderer.submit();}};
        if(width<=0||height<=0||(SDL_GetWindowFlags(window)&(SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN))){offscreen();return Outcome::paused;}
        if(dirty||extent.width!=uint32_t(width)||extent.height!=uint32_t(height))if(!resize(uint32_t(width),uint32_t(height))){offscreen();return Outcome::paused;}
        renderer.wait_next();auto &slot=slots[renderer.next_slot()];uint32_t index=0;
        const auto acquired=vkAcquireNextImageKHR(device,swapchain,1000000000ULL,slot.acquired,VK_NULL_HANDLE,&index);
        if(acquired==VK_ERROR_OUT_OF_DATE_KHR){dirty=true;offscreen();return Outcome::recreate;}
        if(acquired==VK_TIMEOUT||acquired==VK_NOT_READY){offscreen();return Outcome::paused;}
        if(acquired!=VK_SUBOPTIMAL_KHR)check(acquired,"acquire image");else dirty=true;
        const auto frame=update_game&&has_game?renderer.encode(video,video.width(),rt::Video::H,scale,scanlines):renderer.begin_cached(has_game);
        const auto native_width=frame.native_extent.width?frame.native_extent.width:496u,native_height=frame.native_extent.height?frame.native_extent.height:384u;
        float scale_factor=std::min(float(extent.width)/native_width,float(extent.height)/native_height);
        if(integer_scale&&scale_factor>=1)scale_factor=std::floor(scale_factor);
        const uint32_t vw=std::max(1u,uint32_t(native_width*scale_factor)),vh=std::max(1u,uint32_t(native_height*scale_factor));
        const uint32_t x=(extent.width-vw)/2,y=(extent.height-vh)/2;
        if(frame.image){
        Uniforms uniform{{float(x),float(y),float(vw),float(vh)},{float(frame.extent.width),float(frame.extent.height),float(frame.native_extent.width),float(frame.native_extent.height)},{0,0,0,0},{0,0,0,0}};
        if(settings.enabled){uniform.strength[0]=strength(settings.scanlines,1);uniform.strength[1]=strength(settings.mask,1);uniform.strength[2]=strength(settings.glow,.5f);uniform.strength[3]=strength(settings.curvature,.15f);uniform.extra[0]=strength(settings.chromatic,1);}
        void *mapped{};check(vkMapMemory(device,slot.memory,0,sizeof uniform,0,&mapped),"map presentation uniform");std::memcpy(mapped,&uniform,sizeof uniform);vkUnmapMemory(device,slot.memory);
        VkDescriptorImageInfo image{samplers[settings.enabled||linear_filter?1:0],frame.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=slot.sets[2];write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        }
        const bool plain_blit=frame.image&&!settings.enabled;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout=plain_blit?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.dstAccessMask=plain_blit?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=images[index];barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
            plain_blit?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        if(plain_blit){
            // Match the production SDL Vulkan blit primitive exactly when CRT
            // is disabled, including hardware nearest/linear filtering.
            VkClearColorValue black{};black.float32[3]=1;
            vkCmdClearColorImage(frame.command,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&black,1,&barrier.subresourceRange);
            barrier.oldLayout=barrier.newLayout;barrier.srcAccessMask=barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkImageMemoryBarrier source{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};source.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;source.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            source.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;source.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;source.srcQueueFamilyIndex=source.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;source.image=frame.image;source.subresourceRange=barrier.subresourceRange;
            vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&source);
            VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            region.srcOffsets[1]={int32_t(frame.extent.width),int32_t(frame.extent.height),1};
            region.dstOffsets[0]={int32_t(x),int32_t(y),0};region.dstOffsets[1]={int32_t(x+vw),int32_t(y+vh),1};
            vkCmdBlitImage(frame.command,frame.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,linear_filter?VK_FILTER_LINEAR:VK_FILTER_NEAREST);
            source.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;source.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;source.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;source.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&source);
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        }
        VkClearValue clear{};clear.color.float32[3]=1;VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=plain_blit?load_pass:pass;rp.framebuffer=framebuffers[index];rp.renderArea={{0,0},extent};rp.clearValueCount=1;rp.pClearValues=&clear;
        vkCmdBeginRenderPass(frame.command,&rp,VK_SUBPASS_CONTENTS_INLINE);if(frame.image&&!plain_blit){vkCmdBindPipeline(frame.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        const VkViewport viewport{float(x),float(y+vh),float(vw),-float(vh),0,1};const VkRect2D clip{{int32_t(x),int32_t(y)},{vw,vh}};vkCmdSetViewport(frame.command,0,1,&viewport);vkCmdSetScissor(frame.command,0,1,&clip);
        vkCmdBindDescriptorSets(frame.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,4,slot.sets.data(),0,nullptr);vkCmdDraw(frame.command,3,1,0,0);}
        if(overlay)overlay(frame.command);vkCmdEndRenderPass(frame.command);
        barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|(plain_blit?VK_ACCESS_TRANSFER_WRITE_BIT:0);barrier.dstAccessMask=0;
        vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|(plain_blit?VK_PIPELINE_STAGE_TRANSFER_BIT:0),VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        renderer.submit(slot.acquired,finished[index],VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&finished[index];present.swapchainCount=1;present.pSwapchains=&swapchain;present.pImageIndices=&index;
        const auto result=vkQueuePresentKHR(renderer.queue(),&present);
        if(result==VK_ERROR_OUT_OF_DATE_KHR||result==VK_SUBOPTIMAL_KHR)dirty=true;else check(result,"present");
        ++stats.presentations;return Outcome::presented;
    }
};
VulkanPresenter::VulkanPresenter():impl_(std::make_unique<Impl>()){}
VulkanPresenter::~VulkanPresenter()=default;
bool VulkanPresenter::init(SDL_Window *window,bool require_software,bool validation){
    try{
        if(!window)throw std::runtime_error("Missing Vulkan window");auto next=std::make_unique<Impl>();next->window=window;
        uint32_t count=0;const auto extensions=SDL_Vulkan_GetInstanceExtensions(&count);if(!extensions)throw std::runtime_error(SDL_GetError());
        VulkanModel2::Config config;config.require_software=require_software;config.validation=validation;config.instance_extensions.assign(extensions,extensions+count);
        config.create_surface=[window](VkInstance instance){VkSurfaceKHR surface{};if(!SDL_Vulkan_CreateSurface(window,instance,nullptr,&surface))throw std::runtime_error(SDL_GetError());return surface;};
        next->renderer.init(config);next->device=next->renderer.device();next->init_resources();
        int width=0,height=0;if(SDL_GetWindowSizeInPixels(window,&width,&height)&&width>0&&height>0)next->resize(uint32_t(width),uint32_t(height));
        impl_=std::move(next);return true;
    }catch(const std::exception &error){impl_->error=error.what();return false;}
}
VulkanPresenter::Outcome VulkanPresenter::present(const rt::Video &video,int scale,bool scanlines,const CrtSettings &settings,bool update_game,bool has_game,const std::function<void(VkCommandBuffer)> &overlay,bool linear_filter,bool integer_scale){
    try{return impl_->draw(video,scale,scanlines,settings,update_game,has_game,overlay,linear_filter,integer_scale);}
    catch(const VulkanError &error){impl_->error=error.what();if(error.result==VK_ERROR_DEVICE_LOST){impl_->lost=true;return Outcome::device_lost;}impl_->failed=true;return Outcome::error;}
    catch(const std::exception &error){impl_->error=error.what();impl_->failed=true;return Outcome::error;}
}
void VulkanPresenter::drain(){impl_->renderer.drain();}
bool VulkanPresenter::capture(const std::filesystem::path &path){
    try{
        const auto frame=impl_->renderer.readback();std::ofstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("Cannot open Vulkan capture: "+path.string());
        file<<"P6\n"<<frame.extent.width<<" "<<frame.extent.height<<"\n255\n";
        for(const auto pixel:frame.pixels){const char rgb[]={char(pixel>>16),char(pixel>>8),char(pixel)};file.write(rgb,sizeof rgb);}
        file.flush();if(!file)throw std::runtime_error("Cannot write Vulkan capture: "+path.string());
        uint64_t hash=0xcbf29ce484222325ULL;const auto *bytes=reinterpret_cast<const uint8_t *>(frame.pixels.data());
        for(size_t i=0;i<frame.pixels.size()*4;++i){hash^=bytes[i];hash*=0x100000001b3ULL;}
        std::fprintf(stderr,"GPU frame capture: %s BGRA hash=%016llx\n",path.string().c_str(),static_cast<unsigned long long>(hash));return true;
    }catch(const VulkanError &error){impl_->error=error.what();if(error.result==VK_ERROR_DEVICE_LOST)impl_->lost=true;return false;}
    catch(const std::exception &error){impl_->error=error.what();return false;}
}
const std::string &VulkanPresenter::error()const{return impl_->error;}
VulkanPresenter::Statistics VulkanPresenter::statistics()const{return impl_->stats;}
VulkanPresenter::UIContext VulkanPresenter::ui_context()const{const auto &s=*impl_;return {s.renderer.instance(),s.renderer.physical_device(),s.device,s.renderer.queue(),s.renderer.queue_family(),s.pass,s.minimum_images,uint32_t(s.images.size()),s.stats.swapchain_creations};}
const VulkanModel2 &VulkanPresenter::renderer()const{return impl_->renderer;}
} // namespace app::native
