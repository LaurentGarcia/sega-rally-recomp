// Original ROM-free native Vulkan offscreen CRT fixture. No SDL dependency.
// Reuses the same production SPIR-V, never game images or recompiled programs.
#include <vulkan/vulkan.h>
#include "app/gpu/crt_shaders_gen.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace {
constexpr uint32_t W=64,H=48,BYTES=W*H*4;
void check(VkResult result,const char *what) {
    if(result!=VK_SUCCESS)throw std::runtime_error(std::string(what)+": Vulkan result "+std::to_string(result));
}
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};};
struct Image {VkImage handle{};VkDeviceMemory memory{};VkImageView view{};};
struct Fixture {
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
    Buffer upload,readback,uniform;Image source,target;VkSampler sampler{};
    std::array<VkDescriptorSetLayout,4> layouts{};VkDescriptorPool descriptors{};
    std::array<VkDescriptorSet,4> sets{};VkPipelineLayout pipeline_layout{};VkPipeline pipeline{};
    VkRenderPass renderpass{};VkFramebuffer framebuffer{};VkShaderModule vs{},ps{};
    VkPhysicalDeviceProperties properties{};
    ~Fixture() {
        if(device) {
            vkDeviceWaitIdle(device);
            if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(renderpass)vkDestroyRenderPass(device,renderpass,nullptr);
            if(pipeline_layout)vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
            for(auto l:layouts)if(l)vkDestroyDescriptorSetLayout(device,l,nullptr);
            if(vs)vkDestroyShaderModule(device,vs,nullptr);
            if(ps)vkDestroyShaderModule(device,ps,nullptr);
            if(sampler)vkDestroySampler(device,sampler,nullptr);
            for(auto *image:{&source,&target}) {
                if(image->view)vkDestroyImageView(device,image->view,nullptr);
                if(image->handle)vkDestroyImage(device,image->handle,nullptr);
                if(image->memory)vkFreeMemory(device,image->memory,nullptr);
            }
            for(auto *b:{&upload,&readback,&uniform}) {
                if(b->handle)vkDestroyBuffer(device,b->handle,nullptr);
                if(b->memory)vkFreeMemory(device,b->memory,nullptr);
            }
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
    uint32_t memory_type(uint32_t bits,VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties p{};vkGetPhysicalDeviceMemoryProperties(physical,&p);
        for(uint32_t i=0;i<p.memoryTypeCount;++i)if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("No compatible Vulkan memory type");
    }
    void make_buffer(Buffer &b,VkDeviceSize bytes,VkBufferUsageFlags usage) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=bytes;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&info,nullptr,&b.handle),"create buffer");
        VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
        alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device,&alloc,nullptr,&b.memory),"allocate buffer");
        check(vkBindBufferMemory(device,b.handle,b.memory,0),"bind buffer");
    }
    void make_image(Image &image,VkImageUsageFlags usage) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=VK_FORMAT_R8G8B8A8_UNORM;
        info.extent={W,H,1};info.mipLevels=1;info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(device,&info,nullptr,&image.handle),"create image");
        VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,image.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
        alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,0);
        check(vkAllocateMemory(device,&alloc,nullptr,&image.memory),"allocate image");
        check(vkBindImageMemory(device,image.handle,image.memory,0),"bind image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=image.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view.format=info.format;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        check(vkCreateImageView(device,&view,nullptr,&image.view),"image view");
    }
    VkShaderModule module(const unsigned char *bytes,size_t size) {
        // Byte arrays have no guaranteed uint32 alignment. Copy before Vulkan.
        std::vector<uint32_t> words(size/4);std::memcpy(words.data(),bytes,size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=size;info.pCode=words.data();
        VkShaderModule result{};check(vkCreateShaderModule(device,&info,nullptr,&result),"shader module");return result;
    }
    void begin() {
        check(vkResetCommandBuffer(command,0),"reset command");
        VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command,&info),"begin command");
    }
    void submit() {
        check(vkEndCommandBuffer(command),"end command");check(vkResetFences(device,1,&fence),"reset fence");
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command;
        check(vkQueueSubmit(queue,1,&info,fence),"submit");
        check(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL),"five-second draw watchdog");
    }
    void barrier(Image &i,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,
                 VkPipelineStageFlags srcstage,VkPipelineStageFlags dststage) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=before;b.newLayout=after;
        b.srcAccessMask=src;b.dstAccessMask=dst;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=i.handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,srcstage,dststage,0,0,nullptr,0,nullptr,1,&b);
    }
    void init(bool require_software) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Rally native Vulkan ROM-free fixture";app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
        check(vkCreateInstance(&ci,nullptr,&instance),"create instance");
        uint32_t count=0;check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"count devices");
        std::vector<VkPhysicalDevice> devices(count);check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"enumerate devices");
        for(auto d:devices) {
            VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(d,&p);
            if(require_software&&p.deviceType!=VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
            uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);
            std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(d,&n,q.data());
            for(uint32_t i=0;i<n;++i)if(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){physical=d;family=i;properties=p;break;}
            if(physical)break;
        }
        if(!physical)throw std::runtime_error("No requested Vulkan graphics device");
        if(properties.apiVersion<VK_API_VERSION_1_1)throw std::runtime_error("Vulkan 1.1 required for native viewport orientation");
        std::printf("Vulkan device: %s; type=%u; API=%u.%u.%u\n",properties.deviceName,unsigned(properties.deviceType),
                    VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),VK_VERSION_PATCH(properties.apiVersion));
        const float priority=1;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        check(vkCreateDevice(physical,&di,nullptr,&device),"create device");vkGetDeviceQueue(device,family,0,&queue);
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=family;
        check(vkCreateCommandPool(device,&pi,nullptr,&pool),"command pool");
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        check(vkAllocateCommandBuffers(device,&ai,&command),"command buffer");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(device,&fi,nullptr,&fence),"fence");
        make_buffer(upload,BYTES,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);make_buffer(readback,BYTES,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        make_buffer(uniform,64,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        make_image(source,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        make_image(target,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;
        si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(device,&si,nullptr,&sampler),"sampler");
        for(unsigned i=0;i<4;++i) {
            VkDescriptorSetLayoutBinding binding{};binding.binding=0;binding.descriptorCount=1;binding.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.descriptorType=i==2?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=i<2?0:1;li.pBindings=&binding;
            check(vkCreateDescriptorSetLayout(device,&li,nullptr,&layouts[i]),"descriptor layout");
        }
        const VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}};
        VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=4;dpi.poolSizeCount=2;dpi.pPoolSizes=sizes;
        check(vkCreateDescriptorPool(device,&dpi,nullptr,&descriptors),"descriptor pool");
        VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dai.descriptorPool=descriptors;dai.descriptorSetCount=4;dai.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(device,&dai,sets.data()),"descriptor sets");
        VkDescriptorImageInfo imageinfo{sampler,source.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorBufferInfo bufferinfo{uniform.handle,0,64};
        std::array<VkWriteDescriptorSet,2> writes{};
        for(auto &w:writes){w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstBinding=0;w.descriptorCount=1;}
        writes[0].dstSet=sets[2];writes[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[0].pImageInfo=&imageinfo;
        writes[1].dstSet=sets[3];writes[1].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[1].pBufferInfo=&bufferinfo;
        vkUpdateDescriptorSets(device,2,writes.data(),0,nullptr);
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=4;pli.pSetLayouts=layouts.data();
        check(vkCreatePipelineLayout(device,&pli,nullptr,&pipeline_layout),"pipeline layout");
        VkAttachmentDescription attachment{};attachment.format=VK_FORMAT_R8G8B8A8_UNORM;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;
        VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=1;ri.pAttachments=&attachment;ri.subpassCount=1;ri.pSubpasses=&sub;
        check(vkCreateRenderPass(device,&ri,nullptr,&renderpass),"render pass");
        VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fbi.renderPass=renderpass;fbi.attachmentCount=1;fbi.pAttachments=&target.view;fbi.width=W;fbi.height=H;fbi.layers=1;
        check(vkCreateFramebuffer(device,&fbi,nullptr,&framebuffer),"framebuffer");
        vs=module(k_vs_crt_spv,sizeof k_vs_crt_spv);ps=module(k_ps_crt_spv,sizeof k_ps_crt_spv);
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(auto &s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=ps;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Negative height implements the same top-left native source convention.
        VkViewport viewport{0,float(H),float(W),-float(H),0,1};VkRect2D scissor{{0,0},{W,H}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;
        gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;
        gp.layout=pipeline_layout;gp.renderPass=renderpass;
        check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline),"CRT graphics pipeline");
    }
    std::vector<uint8_t> pixels() {
        std::vector<uint8_t> p(BYTES);
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x){auto i=(y*W+x)*4;p[i]=uint8_t(20+x*3);p[i+1]=uint8_t(40+y*4);p[i+2]=100;p[i+3]=255;}
        return p;
    }
    void upload_source(const std::vector<uint8_t> &pixels) {
        void *mapped{};check(vkMapMemory(device,upload.memory,0,BYTES,0,&mapped),"map upload");std::memcpy(mapped,pixels.data(),BYTES);vkUnmapMemory(device,upload.memory);
        begin();barrier(source,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={W,H,1};
        vkCmdCopyBufferToImage(command,upload.handle,source.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier(source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);submit();
    }
    std::vector<uint8_t> draw(float mask,bool first) {
        const std::array<float,16> u={0,0,float(W),float(H),float(W),float(H),float(W),float(H),0,mask,0,0,0,0,0,0};
        void *mapped{};check(vkMapMemory(device,uniform.memory,0,64,0,&mapped),"map uniforms");std::memcpy(mapped,u.data(),64);vkUnmapMemory(device,uniform.memory);
        begin();barrier(target,first?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        first?VkAccessFlags(0):VkAccessFlags(VK_ACCESS_TRANSFER_READ_BIT),VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        first?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkClearValue clear{};clear.color.float32[0]=1;clear.color.float32[2]=1;
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=renderpass;ri.framebuffer=framebuffer;ri.renderArea={{0,0},{W,H}};ri.clearValueCount=1;ri.pClearValues=&clear;
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,4,sets.data(),0,nullptr);
        vkCmdDraw(command,3,1,0,0);vkCmdEndRenderPass(command);
        barrier(target,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={W,H,1};
        vkCmdCopyImageToBuffer(command,target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);
        VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.buffer=readback.handle;b.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);submit();
        check(vkMapMemory(device,readback.memory,0,BYTES,0,&mapped),"map readback");
        std::vector<uint8_t> result(BYTES);std::memcpy(result.data(),mapped,BYTES);vkUnmapMemory(device,readback.memory);return result;
    }
};
}
int main(int argc,char **argv) {
    try {
        bool software=false;std::filesystem::path output;
        for(int i=1;i<argc;++i) {
            if(std::strcmp(argv[i],"--require-software")==0)software=true;
            else if(std::strcmp(argv[i],"--output")==0&&i+1<argc)output=argv[++i];
            else throw std::runtime_error("Usage: test_segarally_native_vulkan [--require-software] [--output NEW_DIR]");
        }
        if(!output.empty()&&!std::filesystem::create_directory(output))throw std::runtime_error("Output directory must be new");
        auto save=[&](const char *name,const std::vector<uint8_t> &bytes) {
            if(output.empty())return;
            std::ofstream file(output/name,std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));
            if(!file)throw std::runtime_error("Cannot write raw image evidence");
        };
        Fixture f;f.init(software);const auto input=f.pixels();save("input.rgba",input);f.upload_source(input);
        uint64_t checks=0;
        for(unsigned pass=0;pass<2;++pass) {
            const auto pixels=f.draw(pass?.5f:0.f,pass==0);
            save(pass?"mask.rgba":"identity.rgba",pixels);
            for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)for(unsigned c=0;c<4;++c) {
                const unsigned i=(y*W+x)*4+c;
                const float multiplier=(pass&&c<3&&c!=x%3)?.5f:1.f;
                const int expected=int(std::lround(float(input[i])*multiplier));
                if(std::abs(int(pixels[i])-expected)>1) {
                    std::fprintf(stderr,"pass=%u pixel=%u,%u channel=%u expected=%d actual=%u\n",pass,x,y,c,expected,pixels[i]);
                    throw std::runtime_error("Native Vulkan CRT pixel mismatch");
                }
                ++checks;
            }
        }
        std::printf("PASS: native Vulkan, common production CRT SPIR-V, 2 draws/readbacks, %llu channel checks; no SDL/ROM/game/surface/audio; not performance evidence\n",static_cast<unsigned long long>(checks));
        return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"Native Vulkan fixture: %s\n",e.what());return 1;}
}
