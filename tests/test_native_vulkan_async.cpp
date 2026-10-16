// Force three caller-extended submissions to remain pending while CPU inputs
// change, then compare their separate readbacks against synchronous snapshots.
#include "app/native/vulkan/model2_vulkan.h"
#include "model2_native_fixture.h"
#include <array>
#include <cstring>
#include <cstdio>
namespace {
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
void check(VkResult result,const char *why){if(result!=VK_SUCCESS)throw app::native::VulkanError(result,why);}
struct Pending {
    app::native::VulkanModel2 &renderer;VkEvent gate{};
    std::array<VkBuffer,3> buffers{};std::array<VkDeviceMemory,3> memory{};
    explicit Pending(app::native::VulkanModel2 &r):renderer(r){VkEventCreateInfo info{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(r.device(),&info,nullptr,&gate),"gate");}
    ~Pending(){
        vkSetEvent(renderer.device(),gate);try{renderer.drain();}catch(...){}
        for(size_t i=0;i<buffers.size();++i){if(buffers[i])vkDestroyBuffer(renderer.device(),buffers[i],nullptr);if(memory[i])vkFreeMemory(renderer.device(),memory[i],nullptr);}
        vkDestroyEvent(renderer.device(),gate,nullptr);
    }
    void allocate(unsigned slot,VkDeviceSize size){
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        check(vkCreateBuffer(renderer.device(),&info,nullptr,&buffers[slot]),"caller readback buffer");
        VkMemoryRequirements req{};vkGetBufferMemoryRequirements(renderer.device(),buffers[slot],&req);
        VkPhysicalDeviceMemoryProperties properties{};vkGetPhysicalDeviceMemoryProperties(renderer.physical_device(),&properties);uint32_t type=UINT32_MAX;
        for(uint32_t i=0;i<properties.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(properties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){type=i;break;}
        require(type!=UINT32_MAX,"host readback memory");VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=type;
        check(vkAllocateMemory(renderer.device(),&alloc,nullptr,&memory[slot]),"readback memory");check(vkBindBufferMemory(renderer.device(),buffers[slot],memory[slot],0),"readback bind");
    }
    void readback(const app::native::VulkanModel2::EncodedFrame &frame,unsigned slot){
        // The host signals only after all three submissions and input mutations.
        vkCmdWaitEvents(frame.command,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,nullptr,0,nullptr,0,nullptr);
        VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};image.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;image.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;image.image=frame.image;image.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&image);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={frame.extent.width,frame.extent.height,1};
        vkCmdCopyImageToBuffer(frame.command,frame.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers[slot],1,&copy);
        VkBufferMemoryBarrier buffer{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};buffer.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;buffer.dstAccessMask=VK_ACCESS_HOST_READ_BIT;buffer.srcQueueFamilyIndex=buffer.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;buffer.buffer=buffers[slot];buffer.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&buffer,0,nullptr);
        image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;image.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;image.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&image);
    }
};
}
int main(int argc,char **argv){try{
    bool software=false,validation=false;for(int i=1;i<argc;++i){const std::string option=argv[i];if(option=="--require-software")software=true;else if(option=="--require-validation")validation=true;else throw std::runtime_error("unknown async option");}
    app::native::VulkanModel2 reference,renderer;reference.init(software,validation);renderer.init(software,validation);
    std::array<std::unique_ptr<model2_fixture::Scene>,3> scenes;
    std::array<std::vector<rt::GeoPoly>,3> geometry;std::array<std::vector<uint32_t>,3> expected;
    for(unsigned i=0;i<3;++i){
        scenes[i]=std::make_unique<model2_fixture::Scene>();auto &scene=*scenes[i];scene.patterned_tiles(i!=1);scene.cutout(i!=0);
        geometry[i]=scene.polys(i==0?0u:i==1?3u:4u);scene.video.set_external_3d(true,true);scene.video.frame_start();scene.video.screen_update(geometry[i],0,scene.mem);
        expected[i]=reference.render(scene.video,496,384,1,true);
    }
    require(expected[0]!=expected[1]&&expected[1]!=expected[2],"three distinct snapshots");
    // Prewarm all three slots and caller allocations before intentionally stopping
    // the queue; memory allocation itself may synchronize software drivers.
    for(unsigned i=0;i<3;++i)renderer.render(scenes[i]->video,496,384,1,true);
    Pending pending(renderer);for(unsigned i=0;i<3;++i)pending.allocate(i,496*384*4);
    const auto initial=renderer.statistics();
    for(unsigned i=0;i<3;++i){
        auto frame=renderer.encode(scenes[i]->video,496,384,1,true);require(frame.slot==i,"three independent frame slots");pending.readback(frame,i);renderer.submit();
        // Destroy/mutate original CPU inputs before allowing any gated copy.
        geometry[i].clear();std::fill(scenes[i]->texture.begin(),scenes[i]->texture.end(),0xdeadbeefu);std::fill(scenes[i]->chars.begin(),scenes[i]->chars.end(),0);
    }
    require(renderer.statistics().submissions==initial.submissions+3&&vkGetEventStatus(renderer.device(),pending.gate)==VK_EVENT_RESET,"three submissions pending before host signal");
    require(renderer.statistics().buffer_allocations==initial.buffer_allocations&&renderer.statistics().image_allocations==initial.image_allocations,"queued frames reuse warmed resources");
    check(vkSetEvent(renderer.device(),pending.gate),"release all pending frames");renderer.drain();
    for(unsigned i=0;i<3;++i){void *bytes{};check(vkMapMemory(renderer.device(),pending.memory[i],0,expected[i].size()*4,0,&bytes),"read async result");const bool equal=std::memcmp(bytes,expected[i].data(),expected[i].size()*4)==0;vkUnmapMemory(renderer.device(),pending.memory[i]);require(equal,"asynchronous frame snapshot remains exact");}
    require(renderer.readback().pixels==expected[2],"cached readback remains exact after source destruction");
    require(renderer.readback().pixels==expected[2],"repeated cached capture preserves image layout");
    require(renderer.statistics().pipeline_creations==4,"persistent model2 pipelines");require(renderer.validation_errors()==0&&reference.validation_errors()==0,"zero validation errors");
    std::puts("PASS three gated Vulkan frames survive source destruction; 571392 exact pixels; four persistent pipelines");return 0;
}catch(const std::exception &error){std::fprintf(stderr,"Vulkan async fixture: %s\n",error.what());return 1;}}
