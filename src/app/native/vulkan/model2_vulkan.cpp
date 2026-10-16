#include "app/native/vulkan/model2_vulkan.h"
#include "app/gpu/model2_resources.h"
#include "app/gpu/shaders_gen.h"
#include <vulkan/vulkan.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace app::native {
namespace {
void checked(VkResult result,const char *operation){if(result!=VK_SUCCESS)throw VulkanError(result,operation);}
struct Buffer{VkBuffer handle{};VkDeviceMemory memory{};size_t size=0;VkBufferUsageFlags usage=0;};
struct Image{VkImage handle{};VkDeviceMemory memory{};VkImageView view{};uint32_t width=0,height=0;VkFormat format=VK_FORMAT_UNDEFINED;};
struct QuadVertex{float x,y,u,v;};
// Destruction is legal only after the owning slot fence (or device shutdown).
struct Snapshot {
    VkDevice device{};size_t buffer_cursor=0,image_cursor=0;
    std::vector<Buffer> buffers;std::vector<Image> images;std::vector<VkShaderModule> modules;
    std::vector<VkDescriptorSetLayout> layouts;std::vector<VkPipelineLayout> pipeline_layouts;std::vector<VkPipeline> pipelines;
    VkDescriptorPool descriptors{};VkRenderPass pass{};VkFramebuffer framebuffer{};VkSampler sampler{};
    explicit Snapshot(VkDevice d):device(d){}
    ~Snapshot(){
        for(auto p:pipelines)if(p)vkDestroyPipeline(device,p,nullptr);
        if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
        if(pass)vkDestroyRenderPass(device,pass,nullptr);
        for(auto p:pipeline_layouts)if(p)vkDestroyPipelineLayout(device,p,nullptr);
        if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
        for(auto p:layouts)if(p)vkDestroyDescriptorSetLayout(device,p,nullptr);
        if(sampler)vkDestroySampler(device,sampler,nullptr);
        for(auto p:modules)if(p)vkDestroyShaderModule(device,p,nullptr);
        for(auto i:images){if(i.view)vkDestroyImageView(device,i.view,nullptr);if(i.handle)vkDestroyImage(device,i.handle,nullptr);if(i.memory)vkFreeMemory(device,i.memory,nullptr);}
        for(auto b:buffers){if(b.handle)vkDestroyBuffer(device,b.handle,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);}
    }
};
}
struct VulkanModel2::Impl {
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family{};
    VkDebugUtilsMessengerEXT debug{};std::atomic<uint32_t> errors{0};VkPhysicalDeviceLimits limits{};
    struct Slot {
        std::unique_ptr<Snapshot> resources,readback_resources;VkCommandBuffer command{};VkFence fence{},last_reader{};
        VkDescriptorSet empty_set{},uniform_set{},poly_set{},tile_set{},quad_set{};
    };
    VkSurfaceKHR surface{};VkCommandPool pool{};VkCommandBuffer command{};
    std::array<Slot,frames_in_flight> slots;uint32_t current_slot=0;bool recording=false;
    std::unique_ptr<Snapshot> immutable;Statistics stats;EncodedFrame cached{};uint32_t pending_reader=frames_in_flight;
    VkDescriptorSetLayout empty_layout{},uniform_layout{},poly_layout{},tile_layout{},quad_layout{};
    VkPipelineLayout poly_pipeline_layout{},tile_pipeline_layout{},quad_pipeline_layout{};
    VkPipeline poly_pipeline{},back_pipeline{},front_pipeline{},quad_pipeline{};
    Model2Frame frame;std::string name;
    ~Impl(){
        if(device){
            vkDeviceWaitIdle(device);
            for(auto &slot:slots){if(slot.fence)vkDestroyFence(device,slot.fence,nullptr);slot.resources.reset();slot.readback_resources.reset();}
            immutable.reset();if(pool)vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);
        }
        if(surface)vkDestroySurfaceKHR(instance,surface,nullptr);
        if(debug){auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));if(destroy)destroy(instance,debug,nullptr);}
        if(instance)vkDestroyInstance(instance,nullptr);
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL diagnostic(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT *message,void *context){
        if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++static_cast<Impl *>(context)->errors;
        std::fprintf(stderr,"Vulkan validation: %s\n",message->pMessage);return VK_FALSE;
    }
    uint32_t memory_type(uint32_t bits,VkMemoryPropertyFlags flags){
        VkPhysicalDeviceMemoryProperties p{};vkGetPhysicalDeviceMemoryProperties(physical,&p);
        for(uint32_t i=0;i<p.memoryTypeCount;++i)if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("No compatible Vulkan memory type");
    }
    Buffer buffer(Snapshot &s,const void *bytes,size_t size,VkBufferUsageFlags usage){
        const size_t content_size=size;size=std::max(size,size_t(16));
        if(s.buffer_cursor==s.buffers.size())s.buffers.emplace_back();auto &b=s.buffers[s.buffer_cursor++];
        if(b.handle&&(b.size<size||b.usage!=usage)){vkDestroyBuffer(device,b.handle,nullptr);vkFreeMemory(device,b.memory,nullptr);b={};}
        if(!b.handle){
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&info,nullptr,&b.handle),"create buffer");VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
        alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        checked(vkAllocateMemory(device,&alloc,nullptr,&b.memory),"allocate buffer");checked(vkBindBufferMemory(device,b.handle,b.memory,0),"bind buffer");b.size=size;b.usage=usage;++stats.buffer_allocations;
        }
        void *mapped{};checked(vkMapMemory(device,b.memory,0,size,0,&mapped),"map buffer");std::memset(mapped,0,size);if(bytes&&content_size)std::memcpy(mapped,bytes,content_size);vkUnmapMemory(device,b.memory);
        return b;
    }
    Image image(Snapshot &s,uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect){
        if(s.image_cursor==s.images.size())s.images.emplace_back();auto &i=s.images[s.image_cursor++];
        if(i.handle&&(i.width!=w||i.height!=h||i.format!=format)){
            vkDestroyImageView(device,i.view,nullptr);vkDestroyImage(device,i.handle,nullptr);vkFreeMemory(device,i.memory,nullptr);i={};
        }
        if(i.handle)return i;
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={w,h,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateImage(device,&info,nullptr,&i.handle),"create image");VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,i.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,0);
        checked(vkAllocateMemory(device,&alloc,nullptr,&i.memory),"allocate image");checked(vkBindImageMemory(device,i.handle,i.memory,0),"bind image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=i.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={aspect,0,1,0,1};
        checked(vkCreateImageView(device,&view,nullptr,&i.view),"image view");i.width=w;i.height=h;i.format=format;++stats.image_allocations;return i;
    }
    VkShaderModule module(Snapshot &s,const unsigned char *bytes,size_t size){
        std::vector<uint32_t> words(size/4);std::memcpy(words.data(),bytes,size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=size;info.pCode=words.data();
        s.modules.emplace_back();checked(vkCreateShaderModule(device,&info,nullptr,&s.modules.back()),"shader module");return s.modules.back();
    }
    VkDescriptorSetLayout layout(Snapshot &s,std::vector<VkDescriptorType> types,VkShaderStageFlags stage){
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for(uint32_t i=0;i<types.size();++i)bindings.push_back({i,types[i],1,stage,nullptr});
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};info.bindingCount=uint32_t(bindings.size());info.pBindings=bindings.data();
        s.layouts.emplace_back();checked(vkCreateDescriptorSetLayout(device,&info,nullptr,&s.layouts.back()),"descriptor layout");return s.layouts.back();
    }
    VkDescriptorSet descriptor(Snapshot &s,VkDescriptorSetLayout layout){
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};info.descriptorPool=s.descriptors;info.descriptorSetCount=1;info.pSetLayouts=&layout;
        VkDescriptorSet result{};checked(vkAllocateDescriptorSets(device,&info,&result),"descriptor set");return result;
    }
    void bind_buffers(VkDescriptorSet set,const std::vector<Buffer> &buffers,VkDescriptorType type){
        std::vector<VkDescriptorBufferInfo> data;std::vector<VkWriteDescriptorSet> writes;
        for(auto b:buffers)data.push_back({b.handle,0,VK_WHOLE_SIZE});
        for(uint32_t i=0;i<data.size();++i){VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};w.dstSet=set;w.dstBinding=i;w.descriptorCount=1;w.descriptorType=type;w.pBufferInfo=&data[i];writes.push_back(w);}
        vkUpdateDescriptorSets(device,uint32_t(writes.size()),writes.data(),0,nullptr);
    }
    VkPipelineLayout pipeline_layout(Snapshot &s,std::array<VkDescriptorSetLayout,3> layouts){
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};info.setLayoutCount=3;info.pSetLayouts=layouts.data();
        s.pipeline_layouts.emplace_back();checked(vkCreatePipelineLayout(device,&info,nullptr,&s.pipeline_layouts.back()),"pipeline layout");return s.pipeline_layouts.back();
    }
    VkPipeline pipeline(Snapshot &s,VkShaderModule vertex,VkShaderModule fragment,VkPipelineLayout layout,bool polygon){
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(auto &p:stages){p.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;p.pName="main";}
        stages[0].module=vertex;stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].module=fragment;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
        const VkFormat formats[]={VK_FORMAT_R32G32_SFLOAT,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32G32B32A32_UINT,VK_FORMAT_R32G32_UINT,VK_FORMAT_R32G32_UINT};
        const uint32_t offsets[]={offsetof(Model2Frame::PolyVertex,x),offsetof(Model2Frame::PolyVertex,depth),offsetof(Model2Frame::PolyVertex,ooz),offsetof(Model2Frame::PolyVertex,r),offsetof(Model2Frame::PolyVertex,info),offsetof(Model2Frame::PolyVertex,more),offsetof(Model2Frame::PolyVertex,edges)};
        std::vector<VkVertexInputAttributeDescription> attributes;
        for(unsigned i=0;i<(polygon?7u:2u);++i)attributes.push_back({i,0,polygon?formats[i]:VK_FORMAT_R32G32_SFLOAT,polygon?offsets[i]:i*8});
        VkVertexInputBindingDescription binding{0,uint32_t(polygon?sizeof(Model2Frame::PolyVertex):sizeof(QuadVertex)),VK_VERTEX_INPUT_RATE_VERTEX};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=uint32_t(attributes.size());vi.pVertexAttributeDescriptions=attributes.data();
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{};VkRect2D scissor{};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.depthClampEnable=VK_TRUE;rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=ds.depthWriteEnable=polygon;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
        const VkDynamicState state[]={VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=state;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=stages.data();info.pVertexInputState=&vi;info.pInputAssemblyState=&ia;info.pViewportState=&vp;info.pRasterizationState=&rs;info.pMultisampleState=&ms;info.pDepthStencilState=&ds;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;info.layout=layout;info.renderPass=s.pass;
        s.pipelines.emplace_back();checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&s.pipelines.back()),"Model2 pipeline");++stats.pipeline_creations;return s.pipelines.back();
    }
    void pipelines_ready(){
        if(immutable)return;
        immutable=std::make_unique<Snapshot>(device);auto &resources=*immutable;
    empty_layout=layout(resources,{},VK_SHADER_STAGE_VERTEX_BIT);
    uniform_layout=layout(resources,{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},VK_SHADER_STAGE_VERTEX_BIT);
    poly_layout=layout(resources,std::vector<VkDescriptorType>(4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),VK_SHADER_STAGE_FRAGMENT_BIT);
    tile_layout=layout(resources,std::vector<VkDescriptorType>(2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),VK_SHADER_STAGE_FRAGMENT_BIT);
    quad_layout=layout(resources,{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},VK_SHADER_STAGE_FRAGMENT_BIT);
    std::array<VkAttachmentDescription,2> attachments{};
    attachments[0].format=VK_FORMAT_B8G8R8A8_UNORM;attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[0].initialLayout=attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1]=attachments[0];attachments[1].format=VK_FORMAT_D16_UNORM;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[1].initialLayout=attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth_ref{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth_ref;
    VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=2;ri.pAttachments=attachments.data();ri.subpassCount=1;ri.pSubpasses=&sub;
    checked(vkCreateRenderPass(device,&ri,nullptr,&resources.pass),"render pass");
    const auto vpoly=module(resources,k_vs_poly_spv,sizeof k_vs_poly_spv),ppoly=module(resources,k_ps_poly_spv,sizeof k_ps_poly_spv),vquad=module(resources,k_vs_quad_spv,sizeof k_vs_quad_spv),pquad=module(resources,k_ps_quad_spv,sizeof k_ps_quad_spv),pback=module(resources,k_ps_tiles_back_spv,sizeof k_ps_tiles_back_spv),pfront=module(resources,k_ps_tiles_front_spv,sizeof k_ps_tiles_front_spv);
    poly_pipeline_layout=pipeline_layout(resources,{empty_layout,uniform_layout,poly_layout});
    tile_pipeline_layout=pipeline_layout(resources,{empty_layout,uniform_layout,tile_layout});
    quad_pipeline_layout=pipeline_layout(resources,{empty_layout,uniform_layout,quad_layout});
    poly_pipeline=pipeline(resources,vpoly,ppoly,poly_pipeline_layout,true);
    back_pipeline=pipeline(resources,vquad,pback,tile_pipeline_layout,false);
    front_pipeline=pipeline(resources,vquad,pfront,tile_pipeline_layout,false);
    quad_pipeline=pipeline(resources,vquad,pquad,quad_pipeline_layout,false);
    }

};
VulkanModel2::VulkanModel2():impl_(std::make_unique<Impl>()){}
VulkanModel2::~VulkanModel2()=default;
const Model2Frame &VulkanModel2::frame()const{return impl_->frame;}
const std::string &VulkanModel2::device_name()const{return impl_->name;}
uint32_t VulkanModel2::validation_errors()const{return impl_->errors.load();}
bool VulkanModel2::init(bool require_software,bool validation){Config config;config.require_software=require_software;config.validation=validation;return init(config);}
bool VulkanModel2::init(const Config &config){
    const bool require_software=config.require_software,validation=config.validation;
    auto next=std::make_unique<Impl>();auto &s=*next;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Rally native Vulkan Model2";app.apiVersion=VK_API_VERSION_1_1;
    const char *layer="VK_LAYER_KHRONOS_validation";
    std::vector<const char *> extensions=config.instance_extensions;
    if(validation){extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);}
    const VkValidationFeatureEnableEXT sync_validation=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation_features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};validation_features.enabledValidationFeatureCount=1;validation_features.pEnabledValidationFeatures=&sync_validation;
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug.pfnUserCallback=Impl::diagnostic;debug.pUserData=&s;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
    if(validation){debug.pNext=&validation_features;ci.pNext=&debug;ci.enabledLayerCount=1;ci.ppEnabledLayerNames=&layer;}
    ci.enabledExtensionCount=uint32_t(extensions.size());ci.ppEnabledExtensionNames=extensions.data();
    checked(vkCreateInstance(&ci,nullptr,&s.instance),"instance (requested validation layer/extensions must be installed)");
    if(validation){
        debug.pNext=nullptr;auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(s.instance,"vkCreateDebugUtilsMessengerEXT"));
        if(!create)throw std::runtime_error("Missing Vulkan debug-utils entry point");
        checked(create(s.instance,&debug,nullptr,&s.debug),"validation messenger");
    }
    if(config.create_surface){s.surface=config.create_surface(s.instance);if(!s.surface)throw std::runtime_error("Surface creation failed");}
    uint32_t count=0;checked(vkEnumeratePhysicalDevices(s.instance,&count,nullptr),"device count");std::vector<VkPhysicalDevice> devices(count);checked(vkEnumeratePhysicalDevices(s.instance,&count,devices.data()),"devices");
    for(auto d:devices){
        VkPhysicalDeviceProperties p{};VkPhysicalDeviceFeatures f{};vkGetPhysicalDeviceProperties(d,&p);vkGetPhysicalDeviceFeatures(d,&f);
        if(p.apiVersion<VK_API_VERSION_1_1||!f.depthClamp||(require_software&&p.deviceType!=VK_PHYSICAL_DEVICE_TYPE_CPU))continue;
        uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(d,&n,q.data());
        for(uint32_t i=0;i<n;++i)if(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){
            if(s.surface){VkBool32 present=VK_FALSE;checked(vkGetPhysicalDeviceSurfaceSupportKHR(d,i,s.surface,&present),"surface support");if(!present)continue;}
            s.physical=d;s.family=i;s.name=p.deviceName;s.limits=p.limits;break;}
        if(s.physical)break;
    }
    if(!s.physical)throw std::runtime_error("No requested Vulkan1.1 graphics device with depthClamp");
    const float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=s.family;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkPhysicalDeviceFeatures features{};features.depthClamp=VK_TRUE;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.pEnabledFeatures=&features;const char *swapchain_extension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if(s.surface){di.enabledExtensionCount=1;di.ppEnabledExtensionNames=&swapchain_extension;}checked(vkCreateDevice(s.physical,&di,nullptr,&s.device),"device");vkGetDeviceQueue(s.device,s.family,0,&s.queue);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=s.family;checked(vkCreateCommandPool(s.device,&pi,nullptr,&s.pool),"command pool");
    for(auto &slot:s.slots){
        slot.resources=std::make_unique<Snapshot>(s.device);
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=s.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        checked(vkAllocateCommandBuffers(s.device,&ai,&slot.command),"command buffer");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;checked(vkCreateFence(s.device,&fi,nullptr,&slot.fence),"fence");
    }
    s.pipelines_ready();impl_=std::move(next);return true;
}
VulkanModel2::EncodedFrame VulkanModel2::encode(const rt::Video &video,int width,int height,int scale,bool scanlines){
    auto &s=*impl_;
    if(!s.device||width<=0||width!=video.width()||height!=rt::Video::H||scale<1||scale>4||
       uint32_t(width)>s.limits.maxImageDimension2D/uint32_t(scale)||uint32_t(height)>s.limits.maxImageDimension2D/uint32_t(scale)||
       uint32_t(width)>s.limits.maxFramebufferWidth/uint32_t(scale)||uint32_t(height)>s.limits.maxFramebufferHeight/uint32_t(scale)||
       uint32_t(width)>s.limits.maxViewportDimensions[0]/uint32_t(scale)||uint32_t(height)>s.limits.maxViewportDimensions[1]/uint32_t(scale))
        throw std::runtime_error("Invalid Vulkan Model2 device/dimensions");
    const auto &mem=video.gpu_mem();
    if(!mem.palram||!mem.colorxlat||!mem.lumaram||!mem.tex0||!mem.tex1)
        throw std::runtime_error("Incomplete Model2 Video memory snapshot");
    s.frame.set_polygon_scanlines(scanlines);s.frame.build(video,width,height,scale);
    wait_next();auto &slot=s.slots[s.current_slot];auto &resources=*slot.resources;s.command=slot.command;
    resources.buffer_cursor=resources.image_cursor=0;const uint32_t w=uint32_t(width*scale),h=uint32_t(height*scale);
    if(resources.framebuffer&&(resources.images[0].width!=w||resources.images[0].height!=h)){
        vkDestroyFramebuffer(s.device,resources.framebuffer,nullptr);resources.framebuffer=VK_NULL_HANDLE;
    }
    const auto empty_layout=s.empty_layout,uniform_layout=s.uniform_layout,poly_layout=s.poly_layout,tile_layout=s.tile_layout,quad_layout=s.quad_layout;
    const auto poly_pipeline_layout=s.poly_pipeline_layout,tile_pipeline_layout=s.tile_pipeline_layout,quad_pipeline_layout=s.quad_pipeline_layout;
    const auto poly_pipeline=s.poly_pipeline,back_pipeline=s.back_pipeline,front_pipeline=s.front_pipeline,quad_pipeline=s.quad_pipeline;
    const auto &vertices=s.frame.vertices();const auto &edges=s.frame.edges();
    const QuadVertex quad[6]={{0,0,0,0},{float(width),0,1,0},{0,float(height),0,1},{float(width),0,1,0},{float(width),float(height),1,1},{0,float(height),0,1}};
    auto qbuf=s.buffer(resources,quad,sizeof quad,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto vbuf=s.buffer(resources,vertices.empty()?nullptr:vertices.data(),vertices.size()*sizeof(Model2Frame::PolyVertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto ebuf=s.buffer(resources,edges.empty()?nullptr:edges.data(),edges.size()*sizeof(Model2Frame::PolyEdge),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::vector<uint8_t> tex(model2_resources::texram_bytes);
    std::memcpy(tex.data(),mem.tex0,model2_resources::sheet_words*4);std::memcpy(tex.data()+model2_resources::sheet_words*4,mem.tex1,model2_resources::sheet_words*4);
    std::vector<uint32_t> xlat(model2_resources::xlat_entries),tiledata(model2_resources::tiledata_words);
    std::vector<uint16_t> tilepix(model2_resources::tilepix_bytes/2);
    const auto gamma=model2_resources::make_gamma();model2_resources::pack_xlat(mem.colorxlat,gamma.data(),xlat.data());
    for(unsigned layer=0;layer<4;++layer)model2_resources::pack_tile_rows(video,layer,0,512,tilepix.data()+layer*512*512);
    model2_resources::pack_tile_data(video,scale,tiledata.data());
    auto texbuf=s.buffer(resources,tex.data(),tex.size(),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto lumabuf=s.buffer(resources,mem.lumaram,model2_resources::luma_bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto xlatbuf=s.buffer(resources,xlat.data(),xlat.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto tilebuf=s.buffer(resources,tilepix.data(),tilepix.size()*2,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto databuf=s.buffer(resources,tiledata.data(),tiledata.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const float screen[4]={float(width),float(height),0,0};auto uniform=s.buffer(resources,screen,sizeof screen,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const auto target=s.image(resources,w,h,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    const auto depth=s.image(resources,w,h,VK_FORMAT_D16_UNORM,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
    if(!resources.descriptors){
        const VkDescriptorPoolSize pool_sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}};
        VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=5;dpi.poolSizeCount=3;dpi.pPoolSizes=pool_sizes;
        checked(vkCreateDescriptorPool(s.device,&dpi,nullptr,&resources.descriptors),"descriptor pool");
        slot.empty_set=s.descriptor(resources,empty_layout);slot.uniform_set=s.descriptor(resources,uniform_layout);
        slot.poly_set=s.descriptor(resources,poly_layout);slot.tile_set=s.descriptor(resources,tile_layout);slot.quad_set=s.descriptor(resources,quad_layout);
    }
    const auto empty_set=slot.empty_set,uniform_set=slot.uniform_set,poly_set=slot.poly_set,tile_set=slot.tile_set,quad_set=slot.quad_set;
    s.bind_buffers(uniform_set,{uniform},VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    s.bind_buffers(poly_set,{texbuf,lumabuf,xlatbuf,ebuf},VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    s.bind_buffers(tile_set,{tilebuf,databuf},VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Image front{};Buffer front_upload{};auto &sampler=resources.sampler;
    if(video.cpu_front()){
        if(video.foreground_layer().size()<size_t(width)*height)throw std::runtime_error("Missing CPU-composed front layer");
        front=s.image(resources,uint32_t(width),uint32_t(height),VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        front_upload=s.buffer(resources,video.foreground_layer().data(),size_t(width)*height*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if(!sampler)checked(vkCreateSampler(s.device,&si,nullptr,&sampler),"front sampler");
        VkDescriptorImageInfo data{sampler,front.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=quad_set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&data;vkUpdateDescriptorSets(s.device,1,&write,0,nullptr);
    }
    if(!resources.framebuffer){
        const VkImageView views[]={target.view,depth.view};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=s.immutable->pass;fi.attachmentCount=2;fi.pAttachments=views;fi.width=w;fi.height=h;fi.layers=1;
        checked(vkCreateFramebuffer(s.device,&fi,nullptr,&resources.framebuffer),"framebuffer");
    }
    checked(vkResetCommandBuffer(s.command,0),"reset command");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(vkBeginCommandBuffer(s.command,&bi),"begin command");
    auto barrier=[&](const Image &image,VkImageAspectFlags aspect,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags srcstage,VkPipelineStageFlags dststage){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;b.oldLayout=before;b.newLayout=after;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image.handle;b.subresourceRange={aspect,0,1,0,1};vkCmdPipelineBarrier(s.command,srcstage,dststage,0,0,nullptr,0,nullptr,1,&b);
    };
    if(video.cpu_front()){
        barrier(front,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(width),uint32_t(height),1};vkCmdCopyBufferToImage(s.command,front_upload.handle,front.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier(front,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    barrier(target,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    barrier(depth,VK_IMAGE_ASPECT_DEPTH_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    std::array<VkClearValue,2> clear{};clear[0].color.float32[3]=1;clear[1].depthStencil.depth=1;
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=s.immutable->pass;rp.framebuffer=resources.framebuffer;rp.renderArea={{0,0},{w,h}};rp.clearValueCount=2;rp.pClearValues=clear.data();vkCmdBeginRenderPass(s.command,&rp,VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0,float(h),float(w),-float(h),0,1};vkCmdSetViewport(s.command,0,1,&viewport);
    auto draw_quad=[&](VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet resource){
        const VkRect2D clip{{0,0},{w,h}};vkCmdSetScissor(s.command,0,1,&clip);vkCmdBindPipeline(s.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        const VkDescriptorSet sets[]={empty_set,uniform_set,resource};vkCmdBindDescriptorSets(s.command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,3,sets,0,nullptr);
        const VkDeviceSize offset=0;vkCmdBindVertexBuffers(s.command,0,1,&qbuf.handle,&offset);vkCmdDraw(s.command,6,1,0,0);
    };
    draw_quad(back_pipeline,tile_pipeline_layout,tile_set);
    if(!vertices.empty()){
        vkCmdBindPipeline(s.command,VK_PIPELINE_BIND_POINT_GRAPHICS,poly_pipeline);const VkDescriptorSet sets[]={empty_set,uniform_set,poly_set};vkCmdBindDescriptorSets(s.command,VK_PIPELINE_BIND_POINT_GRAPHICS,poly_pipeline_layout,0,3,sets,0,nullptr);
        const VkDeviceSize offset=0;vkCmdBindVertexBuffers(s.command,0,1,&vbuf.handle,&offset);
        for(const auto &batch:s.frame.batches())if(batch.count){
            if(batch.clip.x<0||batch.clip.y<0||batch.clip.w<=0||batch.clip.h<=0||size_t(batch.first)+batch.count>vertices.size()||batch.clip.x+batch.clip.w>width||batch.clip.y+batch.clip.h>height)
                throw std::runtime_error("Invalid common Model2 batch");
            const VkRect2D clip{{batch.clip.x*scale,batch.clip.y*scale},{uint32_t(batch.clip.w*scale),uint32_t(batch.clip.h*scale)}};vkCmdSetScissor(s.command,0,1,&clip);vkCmdDraw(s.command,batch.count,1,batch.first,0);
        }
    }
    if(video.cpu_front())draw_quad(quad_pipeline,quad_pipeline_layout,quad_set);else draw_quad(front_pipeline,tile_pipeline_layout,tile_set);
    vkCmdEndRenderPass(s.command);
    barrier(target,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    s.recording=true;s.pending_reader=s.current_slot;s.cached={s.command,target.handle,target.view,{w,h},{uint32_t(width),uint32_t(height)},s.current_slot};return s.cached;
}
uint32_t VulkanModel2::next_slot()const{return impl_->current_slot;}
void VulkanModel2::wait_next(){
    auto &s=*impl_;if(!s.device||s.recording)throw std::runtime_error("Vulkan frame already recording or uninitialized");
    ++s.stats.slot_waits;auto &slot=s.slots[s.current_slot];checked(vkWaitForFences(s.device,1,&slot.fence,VK_TRUE,5000000000ULL),"five-second frame slot fence");
    if(slot.last_reader&&slot.last_reader!=slot.fence)checked(vkWaitForFences(s.device,1,&slot.last_reader,VK_TRUE,5000000000ULL),"cached image reader fence");
    slot.last_reader=VK_NULL_HANDLE;
}
VulkanModel2::EncodedFrame VulkanModel2::begin_cached(bool has_game){
    wait_next();auto &s=*impl_;auto &slot=s.slots[s.current_slot];
    checked(vkResetCommandBuffer(slot.command,0),"reset presentation command");VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checked(vkBeginCommandBuffer(slot.command,&info),"begin presentation command");
    EncodedFrame result=has_game?s.cached:EncodedFrame{};s.pending_reader=result.image?result.slot:frames_in_flight;
    result.command=slot.command;result.slot=s.current_slot;s.recording=true;return result;
}
void VulkanModel2::submit(VkSemaphore acquire,VkSemaphore rendered,VkPipelineStageFlags wait_stage){
    auto &s=*impl_;if(!s.recording)throw std::runtime_error("No Vulkan frame to submit");auto &slot=s.slots[s.current_slot];
    checked(vkEndCommandBuffer(slot.command),"end command");checked(vkResetFences(s.device,1,&slot.fence),"reset fence");
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&slot.command;
    if(acquire){info.waitSemaphoreCount=1;info.pWaitSemaphores=&acquire;info.pWaitDstStageMask=&wait_stage;}
    if(rendered){info.signalSemaphoreCount=1;info.pSignalSemaphores=&rendered;}
    const auto result=vkQueueSubmit(s.queue,1,&info,slot.fence);s.recording=false;checked(result,"submit");
    if(s.pending_reader<frames_in_flight)s.slots[s.pending_reader].last_reader=slot.fence;
    ++s.stats.submissions;s.current_slot=(s.current_slot+1)%frames_in_flight;
}
void VulkanModel2::drain(){auto &s=*impl_;if(s.recording)throw std::runtime_error("Cannot drain while recording");if(s.device)for(auto &slot:s.slots)checked(vkWaitForFences(s.device,1,&slot.fence,VK_TRUE,5000000000ULL),"drain frame slot");}
VulkanModel2::Statistics VulkanModel2::statistics()const{return impl_->stats;}
VkInstance VulkanModel2::instance()const{return impl_->instance;}
VkPhysicalDevice VulkanModel2::physical_device()const{return impl_->physical;}
VkDevice VulkanModel2::device()const{return impl_->device;}
VkQueue VulkanModel2::queue()const{return impl_->queue;}
VkSurfaceKHR VulkanModel2::surface()const{return impl_->surface;}
uint32_t VulkanModel2::queue_family()const{return impl_->family;}
std::vector<uint32_t> VulkanModel2::render(const rt::Video &video,int width,int height,int scale,bool scanlines){
    return readback_encoded(encode(video,width,height,scale,scanlines)).pixels;
}
VulkanModel2::Readback VulkanModel2::readback(){
    if(!impl_->cached.image)throw std::runtime_error("No Vulkan game frame to capture");
    return readback_encoded(begin_cached());
}
VulkanModel2::Readback VulkanModel2::readback_encoded(const EncodedFrame &frame){
    auto &s=*impl_;auto &slot=s.slots[frame.slot];
    if(!slot.readback_resources)slot.readback_resources=std::make_unique<Snapshot>(s.device);
    auto &diagnostic=*slot.readback_resources;diagnostic.buffer_cursor=0;
    auto readback=s.buffer(diagnostic,nullptr,size_t(frame.extent.width)*frame.extent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};image.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;image.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;image.image=frame.image;image.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&image);
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={frame.extent.width,frame.extent.height,1};vkCmdCopyImageToBuffer(frame.command,frame.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=readback.handle;barrier.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    // Keep the cached image sampleable after an optional diagnostic capture.
    image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;image.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;image.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(frame.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&image);
    submit();checked(vkWaitForFences(s.device,1,&slot.fence,VK_TRUE,5000000000ULL),"diagnostic readback fence");
    std::vector<uint32_t> result(size_t(frame.extent.width)*frame.extent.height);void *mapped{};
    checked(vkMapMemory(s.device,readback.memory,0,result.size()*4,0,&mapped),"map readback");std::memcpy(result.data(),mapped,result.size()*4);vkUnmapMemory(s.device,readback.memory);return {std::move(result),frame.extent,frame.native_extent};
}
} // namespace app::native
