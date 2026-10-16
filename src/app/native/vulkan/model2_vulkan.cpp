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
void checked(VkResult result,const char *operation){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(operation)+": Vulkan result "+std::to_string(result));}
struct Buffer{VkBuffer handle{};VkDeviceMemory memory{};};
struct Image{VkImage handle{};VkDeviceMemory memory{};VkImageView view{};};
struct QuadVertex{float x,y,u,v;};
// All resources stay alive until this submission is complete, including errors.
struct Snapshot {
    VkDevice device{};
    std::vector<Buffer> buffers;std::vector<Image> images;std::vector<VkShaderModule> modules;
    std::vector<VkDescriptorSetLayout> layouts;std::vector<VkPipelineLayout> pipeline_layouts;std::vector<VkPipeline> pipelines;
    VkDescriptorPool descriptors{};VkRenderPass pass{};VkFramebuffer framebuffer{};VkSampler sampler{};
    explicit Snapshot(VkDevice d):device(d){}
    ~Snapshot(){
        vkDeviceWaitIdle(device);
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
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};Model2Frame frame;std::string name;
    ~Impl(){
        if(device){vkDeviceWaitIdle(device);if(fence)vkDestroyFence(device,fence,nullptr);if(pool)vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);}
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
        s.buffers.emplace_back();auto &b=s.buffers.back();const size_t content_size=size;size=std::max(size,size_t(16));
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&info,nullptr,&b.handle),"create buffer");VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;
        alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        checked(vkAllocateMemory(device,&alloc,nullptr,&b.memory),"allocate buffer");checked(vkBindBufferMemory(device,b.handle,b.memory,0),"bind buffer");
        void *mapped{};checked(vkMapMemory(device,b.memory,0,size,0,&mapped),"map buffer");std::memset(mapped,0,size);if(bytes&&content_size)std::memcpy(mapped,bytes,content_size);vkUnmapMemory(device,b.memory);
        return b;
    }
    Image image(Snapshot &s,uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect){
        s.images.emplace_back();auto &i=s.images.back();VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={w,h,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateImage(device,&info,nullptr,&i.handle),"create image");VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,i.handle,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memory_type(req.memoryTypeBits,0);
        checked(vkAllocateMemory(device,&alloc,nullptr,&i.memory),"allocate image");checked(vkBindImageMemory(device,i.handle,i.memory,0),"bind image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=i.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={aspect,0,1,0,1};
        checked(vkCreateImageView(device,&view,nullptr,&i.view),"image view");return i;
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
    VkPipeline pipeline(Snapshot &s,VkShaderModule vertex,VkShaderModule fragment,VkPipelineLayout layout,bool polygon,unsigned w,unsigned h){
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
        VkViewport viewport{0,float(h),float(w),-float(h),0,1};VkRect2D scissor{{0,0},{w,h}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.depthClampEnable=VK_TRUE;rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=ds.depthWriteEnable=polygon;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
        const VkDynamicState state=VK_DYNAMIC_STATE_SCISSOR;VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=1;dynamic.pDynamicStates=&state;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=stages.data();info.pVertexInputState=&vi;info.pInputAssemblyState=&ia;info.pViewportState=&vp;info.pRasterizationState=&rs;info.pMultisampleState=&ms;info.pDepthStencilState=&ds;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;info.layout=layout;info.renderPass=s.pass;
        s.pipelines.emplace_back();checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&s.pipelines.back()),"Model2 pipeline");return s.pipelines.back();
    }
};
VulkanModel2::VulkanModel2():impl_(std::make_unique<Impl>()){}
VulkanModel2::~VulkanModel2()=default;
const Model2Frame &VulkanModel2::frame()const{return impl_->frame;}
const std::string &VulkanModel2::device_name()const{return impl_->name;}
uint32_t VulkanModel2::validation_errors()const{return impl_->errors.load();}
bool VulkanModel2::init(bool require_software,bool validation){
    auto next=std::make_unique<Impl>();auto &s=*next;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Rally native Vulkan Model2";app.apiVersion=VK_API_VERSION_1_1;
    const char *layer="VK_LAYER_KHRONOS_validation";
    const char *extensions[]={VK_EXT_DEBUG_UTILS_EXTENSION_NAME,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
    const VkValidationFeatureEnableEXT sync_validation=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation_features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};validation_features.enabledValidationFeatureCount=1;validation_features.pEnabledValidationFeatures=&sync_validation;
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug.pfnUserCallback=Impl::diagnostic;debug.pUserData=&s;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
    if(validation){debug.pNext=&validation_features;ci.pNext=&debug;ci.enabledLayerCount=1;ci.ppEnabledLayerNames=&layer;ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=extensions;}
    checked(vkCreateInstance(&ci,nullptr,&s.instance),"instance (requested validation layer/extensions must be installed)");
    if(validation){
        debug.pNext=nullptr;auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(s.instance,"vkCreateDebugUtilsMessengerEXT"));
        if(!create)throw std::runtime_error("Missing Vulkan debug-utils entry point");
        checked(create(s.instance,&debug,nullptr,&s.debug),"validation messenger");
    }
    uint32_t count=0;checked(vkEnumeratePhysicalDevices(s.instance,&count,nullptr),"device count");std::vector<VkPhysicalDevice> devices(count);checked(vkEnumeratePhysicalDevices(s.instance,&count,devices.data()),"devices");
    for(auto d:devices){
        VkPhysicalDeviceProperties p{};VkPhysicalDeviceFeatures f{};vkGetPhysicalDeviceProperties(d,&p);vkGetPhysicalDeviceFeatures(d,&f);
        if(p.apiVersion<VK_API_VERSION_1_1||!f.depthClamp||(require_software&&p.deviceType!=VK_PHYSICAL_DEVICE_TYPE_CPU))continue;
        uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(d,&n,q.data());
        for(uint32_t i=0;i<n;++i)if(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){s.physical=d;s.family=i;s.name=p.deviceName;s.limits=p.limits;break;}
        if(s.physical)break;
    }
    if(!s.physical)throw std::runtime_error("No requested Vulkan1.1 graphics device with depthClamp");
    const float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=s.family;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkPhysicalDeviceFeatures features{};features.depthClamp=VK_TRUE;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.pEnabledFeatures=&features;checked(vkCreateDevice(s.physical,&di,nullptr,&s.device),"device");vkGetDeviceQueue(s.device,s.family,0,&s.queue);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=s.family;checked(vkCreateCommandPool(s.device,&pi,nullptr,&s.pool),"command pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=s.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;checked(vkAllocateCommandBuffers(s.device,&ai,&s.command),"command buffer");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};checked(vkCreateFence(s.device,&fi,nullptr,&s.fence),"fence");impl_=std::move(next);return true;
}
std::vector<uint32_t> VulkanModel2::render(const rt::Video &video,int width,int height,int scale,bool scanlines){
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
    Snapshot resources(s.device);const uint32_t w=uint32_t(width*scale),h=uint32_t(height*scale);
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
    const auto target=s.image(resources,w,h,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    const auto depth=s.image(resources,w,h,VK_FORMAT_D16_UNORM,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
    auto readback=s.buffer(resources,nullptr,size_t(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkDescriptorPoolSize pool_sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=5;dpi.poolSizeCount=3;dpi.pPoolSizes=pool_sizes;
    checked(vkCreateDescriptorPool(s.device,&dpi,nullptr,&resources.descriptors),"descriptor pool");
    const auto empty_layout=s.layout(resources,{},VK_SHADER_STAGE_VERTEX_BIT);
    const auto uniform_layout=s.layout(resources,{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},VK_SHADER_STAGE_VERTEX_BIT);
    const auto poly_layout=s.layout(resources,std::vector<VkDescriptorType>(4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto tile_layout=s.layout(resources,std::vector<VkDescriptorType>(2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto quad_layout=s.layout(resources,{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto empty_set=s.descriptor(resources,empty_layout),uniform_set=s.descriptor(resources,uniform_layout),poly_set=s.descriptor(resources,poly_layout),tile_set=s.descriptor(resources,tile_layout),quad_set=s.descriptor(resources,quad_layout);
    s.bind_buffers(uniform_set,{uniform},VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    s.bind_buffers(poly_set,{texbuf,lumabuf,xlatbuf,ebuf},VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    s.bind_buffers(tile_set,{tilebuf,databuf},VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Image front{};Buffer front_upload{};auto &sampler=resources.sampler;
    if(video.cpu_front()){
        if(video.foreground_layer().size()<size_t(width)*height)throw std::runtime_error("Missing CPU-composed front layer");
        front=s.image(resources,uint32_t(width),uint32_t(height),VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        front_upload=s.buffer(resources,video.foreground_layer().data(),size_t(width)*height*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        checked(vkCreateSampler(s.device,&si,nullptr,&sampler),"front sampler");
        VkDescriptorImageInfo data{sampler,front.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=quad_set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&data;vkUpdateDescriptorSets(s.device,1,&write,0,nullptr);
    }
    std::array<VkAttachmentDescription,2> attachments{};
    attachments[0].format=VK_FORMAT_B8G8R8A8_UNORM;attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[0].initialLayout=attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1]=attachments[0];attachments[1].format=VK_FORMAT_D16_UNORM;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[1].initialLayout=attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth_ref{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth_ref;
    VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=2;ri.pAttachments=attachments.data();ri.subpassCount=1;ri.pSubpasses=&sub;
    checked(vkCreateRenderPass(s.device,&ri,nullptr,&resources.pass),"render pass");
    const VkImageView views[]={target.view,depth.view};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=resources.pass;fi.attachmentCount=2;fi.pAttachments=views;fi.width=w;fi.height=h;fi.layers=1;
    checked(vkCreateFramebuffer(s.device,&fi,nullptr,&resources.framebuffer),"framebuffer");
    const auto vpoly=s.module(resources,k_vs_poly_spv,sizeof k_vs_poly_spv),ppoly=s.module(resources,k_ps_poly_spv,sizeof k_ps_poly_spv),vquad=s.module(resources,k_vs_quad_spv,sizeof k_vs_quad_spv),pquad=s.module(resources,k_ps_quad_spv,sizeof k_ps_quad_spv),pback=s.module(resources,k_ps_tiles_back_spv,sizeof k_ps_tiles_back_spv),pfront=s.module(resources,k_ps_tiles_front_spv,sizeof k_ps_tiles_front_spv);
    const auto poly_pipeline_layout=s.pipeline_layout(resources,{empty_layout,uniform_layout,poly_layout});
    const auto tile_pipeline_layout=s.pipeline_layout(resources,{empty_layout,uniform_layout,tile_layout});
    const auto quad_pipeline_layout=s.pipeline_layout(resources,{empty_layout,uniform_layout,quad_layout});
    const auto poly_pipeline=s.pipeline(resources,vpoly,ppoly,poly_pipeline_layout,true,w,h),back_pipeline=s.pipeline(resources,vquad,pback,tile_pipeline_layout,false,w,h),front_pipeline=s.pipeline(resources,vquad,pfront,tile_pipeline_layout,false,w,h),quad_pipeline=s.pipeline(resources,vquad,pquad,quad_pipeline_layout,false,w,h);
    checked(vkResetCommandBuffer(s.command,0),"reset command");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(vkBeginCommandBuffer(s.command,&bi),"begin command");
    auto barrier=[&](const Image &image,VkImageAspectFlags aspect,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags srcstage,VkPipelineStageFlags dststage){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;b.oldLayout=before;b.newLayout=after;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image.handle;b.subresourceRange={aspect,0,1,0,1};vkCmdPipelineBarrier(s.command,srcstage,dststage,0,0,nullptr,0,nullptr,1,&b);
    };
    if(video.cpu_front()){
        barrier(front,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(width),uint32_t(height),1};vkCmdCopyBufferToImage(s.command,front_upload.handle,front.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier(front,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    barrier(target,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    barrier(depth,VK_IMAGE_ASPECT_DEPTH_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,0,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    std::array<VkClearValue,2> clear{};clear[0].color.float32[3]=1;clear[1].depthStencil.depth=1;
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=resources.pass;rp.framebuffer=resources.framebuffer;rp.renderArea={{0,0},{w,h}};rp.clearValueCount=2;rp.pClearValues=clear.data();vkCmdBeginRenderPass(s.command,&rp,VK_SUBPASS_CONTENTS_INLINE);
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
    barrier(target,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={w,h,1};vkCmdCopyImageToBuffer(s.command,target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);
    VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.buffer=readback.handle;b.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(s.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);
    checked(vkEndCommandBuffer(s.command),"end command");checked(vkResetFences(s.device,1,&s.fence),"reset fence");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&s.command;checked(vkQueueSubmit(s.queue,1,&submit,s.fence),"submit");checked(vkWaitForFences(s.device,1,&s.fence,VK_TRUE,5000000000ULL),"five-second frame fence");
    void *mapped{};checked(vkMapMemory(s.device,readback.memory,0,size_t(w)*h*4,0,&mapped),"map readback");std::vector<uint32_t> result(size_t(w)*h);std::memcpy(result.data(),mapped,result.size()*4);vkUnmapMemory(s.device,readback.memory);return result;
}
} // namespace app::native
