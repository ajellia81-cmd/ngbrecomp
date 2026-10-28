// Headless, synthetic GPU comparisons. No game files, saves, windows or sound.
#include <vulkan/vulkan.h>
#include "gpu/universal_shaders.h"
#include "vk/spirv.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace gpu;
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
static void vk(VkResult result){if(result!=VK_SUCCESS)throw std::runtime_error("Vulkan failure "+std::to_string(result));}
struct Buffer {VkBuffer buffer{};VkDeviceMemory memory{};void *mapped{};VkDeviceSize size{};};
struct Image {VkImage image{};VkImageView view{};VkDeviceMemory memory{};VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;};
struct Vertex {float position[4],color[4],uv[4],normal[4],weights[4];};
class GPU {
public:
 VkInstance instance{};VkDebugUtilsMessengerEXT messenger{};unsigned validation_errors=0;VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};
 VkCommandPool pool{};VkCommandBuffer cmd{};VkDescriptorPool descriptor_pool{};
 VkDescriptorSetLayout universal_layout{},special_layout{};VkPipelineLayout universal_pipeline_layout{},special_pipeline_layout{};
 VkSampler sampler{},comparison{};
 VkBlendFactor src_factor=VK_BLEND_FACTOR_ONE,dst_factor=VK_BLEND_FACTOR_ONE;VkBlendOp blend_op=VK_BLEND_OP_ADD;unsigned write_mask=15;
 bool ordered=false;uint32_t family=0;
 static constexpr unsigned W=48,H=48;
 std::vector<Buffer> allocations;std::vector<Image> images;
 Buffer vc,dc,uc,vertices,scene,readback;
 Image tex2d,tex3d,cube,depth;
 VkShaderModule universal_vs{},universal_ps{},fetch_ps{};
 std::array<VkPipeline,2> universal_pipelines{};
 static VKAPI_ATTR VkBool32 VKAPI_CALL validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void* user){
  if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)static_cast<GPU*>(user)->validation_errors++;
  std::cerr<<data->pMessage<<'\n';return VK_FALSE;
 }
 GPU(){
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;
  VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;debug.pfnUserCallback=validation;debug.pUserData=this;
  const char* instance_extensions[]={VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#ifdef __APPLE__
   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
  };
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;ici.pNext=&debug;ici.enabledExtensionCount=sizeof(instance_extensions)/sizeof(instance_extensions[0]);ici.ppEnabledExtensionNames=instance_extensions;
#ifdef __APPLE__
  ici.flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
  vk(vkCreateInstance(&ici,nullptr,&instance));
  auto create_debug=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");check(create_debug!=nullptr,"validation debug callback unavailable");vk(create_debug(instance,&debug,nullptr,&messenger));
  uint32_t count=0;vk(vkEnumeratePhysicalDevices(instance,&count,nullptr));check(count>0,"no headless Vulkan device");
  std::vector<VkPhysicalDevice> devices(count);vk(vkEnumeratePhysicalDevices(instance,&count,devices.data()));physical=devices[0];
  VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(physical,&properties);std::cout<<"GPU: "<<properties.deviceName<<'\n';
  uint32_t ne=0;vk(vkEnumerateDeviceExtensionProperties(physical,nullptr,&ne,nullptr));std::vector<VkExtensionProperties> ext(ne);vk(vkEnumerateDeviceExtensionProperties(physical,nullptr,&ne,ext.data()));
  auto has=[&](const char *name){return std::any_of(ext.begin(),ext.end(),[&](const auto& e){return !std::strcmp(e.extensionName,name);});};
  check(has(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME),"dynamic rendering local read unavailable");
  ordered=has(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
  VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT order{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT};
  VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR local{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};local.pNext=ordered?&order:nullptr;
  VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};f13.pNext=&local;
  VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&f13;vkGetPhysicalDeviceFeatures2(physical,&features);
  check(f13.dynamicRendering && f13.synchronization2 && local.dynamicRenderingLocalRead,"required rendering features unavailable");
  ordered=ordered && order.rasterizationOrderColorAttachmentAccess;
  uint32_t nq=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&nq,nullptr);std::vector<VkQueueFamilyProperties> queues(nq);vkGetPhysicalDeviceQueueFamilyProperties(physical,&nq,queues.data());
  for(unsigned i=0;i<nq;i++)if(queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){family=i;break;}
  float priority=1;VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;
  std::vector<const char*> extensions{VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME};
#ifdef __APPLE__
  extensions.push_back("VK_KHR_portability_subset");
#endif
  if(ordered)extensions.push_back(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
  VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};create.pNext=&features;create.queueCreateInfoCount=1;create.pQueueCreateInfos=&q;create.enabledExtensionCount=extensions.size();create.ppEnabledExtensionNames=extensions.data();vk(vkCreateDevice(physical,&create,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
  VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=family;vk(vkCreateCommandPool(device,&cp,nullptr,&pool));
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vk(vkAllocateCommandBuffers(device,&ca,&cmd));
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,12},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,8},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,64},{VK_DESCRIPTOR_TYPE_SAMPLER,32},{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,4}};
  VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;dp.maxSets=8;dp.poolSizeCount=5;dp.pPoolSizes=sizes;vk(vkCreateDescriptorPool(device,&dp,nullptr,&descriptor_pool));
  universal_layout=layout(true);special_layout=layout(false);universal_pipeline_layout=pipeline_layout(universal_layout);special_pipeline_layout=pipeline_layout(special_layout);
  VkSamplerCreateInfo sm{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sm.magFilter=sm.minFilter=VK_FILTER_LINEAR;sm.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sm.maxLod=0;sm.addressModeU=sm.addressModeV=sm.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;vk(vkCreateSampler(device,&sm,nullptr,&sampler));sm.compareEnable=VK_TRUE;sm.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;vk(vkCreateSampler(device,&sm,nullptr,&comparison));
  vc=buffer(192*16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);dc=buffer(sizeof(DrawConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);uc=buffer(sizeof(UniversalConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  vertices=buffer(sizeof(Vertex)*12,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);scene=buffer(12*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);readback=buffer(W*H*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  tex2d=image(VK_IMAGE_TYPE_2D,VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_B8G8R8A8_UNORM,1,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  tex3d=image(VK_IMAGE_TYPE_3D,VK_IMAGE_VIEW_TYPE_3D,VK_FORMAT_B8G8R8A8_UNORM,1,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  cube=image(VK_IMAGE_TYPE_2D,VK_IMAGE_VIEW_TYPE_CUBE,VK_FORMAT_B8G8R8A8_UNORM,6,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  depth=image(VK_IMAGE_TYPE_2D,VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_D32_SFLOAT,1,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,true);
  begin();for(auto *i:{&tex2d,&tex3d,&cube}){barrier(*i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);VkClearColorValue color{{.2f,.4f,.6f,.8f}};VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,i==&cube?6u:1u};vkCmdClearColorImage(cmd,i->image,i->layout,&color,1,&range);barrier(*i,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}
  barrier(depth,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,true);VkClearDepthStencilValue value{.5f,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};vkCmdClearDepthStencilImage(cmd,depth.image,depth.layout,&value,1,&range);barrier(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true);submit();
  universal_vs=module(UniversalVertexHlsl(),true);universal_ps=module(UniversalPixelHlsl(false,false,false),false);fetch_ps=module(UniversalPixelHlsl(false,false,true),false);
 }
 ~GPU(){if(device)vkDeviceWaitIdle(device);for(auto p:universal_pipelines)if(p)vkDestroyPipeline(device,p,nullptr);for(auto module:{universal_vs,universal_ps,fetch_ps})if(module)vkDestroyShaderModule(device,module,nullptr);for(auto i:images){vkDestroyImageView(device,i.view,nullptr);vkDestroyImage(device,i.image,nullptr);vkFreeMemory(device,i.memory,nullptr);}for(auto b:allocations){vkUnmapMemory(device,b.memory);vkDestroyBuffer(device,b.buffer,nullptr);vkFreeMemory(device,b.memory,nullptr);}for(auto l:{universal_pipeline_layout,special_pipeline_layout})if(l)vkDestroyPipelineLayout(device,l,nullptr);for(auto l:{universal_layout,special_layout})if(l)vkDestroyDescriptorSetLayout(device,l,nullptr);for(auto s:{sampler,comparison})if(s)vkDestroySampler(device,s,nullptr);if(descriptor_pool)vkDestroyDescriptorPool(device,descriptor_pool,nullptr);if(pool)vkDestroyCommandPool(device,pool,nullptr);if(device)vkDestroyDevice(device,nullptr);if(messenger)((PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"))(instance,messenger,nullptr);if(instance)vkDestroyInstance(instance,nullptr);}
 uint32_t memory_type(uint32_t bits,VkMemoryPropertyFlags flags){VkPhysicalDeviceMemoryProperties p;vkGetPhysicalDeviceMemoryProperties(physical,&p);for(unsigned i=0;i<p.memoryTypeCount;i++)if((bits&(1u<<i)) && (p.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("missing memory type");}
 Buffer buffer(VkDeviceSize bytes,VkBufferUsageFlags usage){Buffer b;b.size=bytes;VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=bytes;ci.usage=usage;vk(vkCreateBuffer(device,&ci,nullptr,&b.buffer));VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.buffer,&req);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vk(vkAllocateMemory(device,&a,nullptr,&b.memory));vk(vkBindBufferMemory(device,b.buffer,b.memory,0));vk(vkMapMemory(device,b.memory,0,bytes,0,&b.mapped));std::memset(b.mapped,0,bytes);allocations.push_back(b);return b;}
 Image image(VkImageType type,VkImageViewType view,VkFormat format,unsigned layers,VkImageUsageFlags usage,bool z=false){Image i;VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=type;ci.flags=view==VK_IMAGE_VIEW_TYPE_CUBE?VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT:0;ci.format=format;ci.extent={W,H,1};ci.mipLevels=1;ci.arrayLayers=layers;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.usage=usage;vk(vkCreateImage(device,&ci,nullptr,&i.image));VkMemoryRequirements req;vkGetImageMemoryRequirements(device,i.image,&req);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=memory_type(req.memoryTypeBits,0);vk(vkAllocateMemory(device,&a,nullptr,&i.memory));vk(vkBindImageMemory(device,i.image,i.memory,0));VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=i.image;vi.viewType=view;vi.format=format;vi.subresourceRange={z?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,layers};vk(vkCreateImageView(device,&vi,nullptr,&i.view));images.push_back(i);return i;}
 void begin(){vk(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo ci{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};ci.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vk(vkBeginCommandBuffer(cmd,&ci));}
 void submit(){vk(vkEndCommandBuffer(cmd));VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cmd;vk(vkQueueSubmit(queue,1,&s,VK_NULL_HANDLE));vk(vkQueueWaitIdle(queue));}
 void barrier(Image& image,VkImageLayout layout,bool z=false){VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};b.srcStageMask=b.dstStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;b.srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;b.oldLayout=image.layout;b.newLayout=layout;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image.image;b.subresourceRange={z?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,&image==&cube?6u:1u};VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.imageMemoryBarrierCount=1;d.pImageMemoryBarriers=&b;vkCmdPipelineBarrier2(cmd,&d);image.layout=layout;}
 VkDescriptorSetLayout layout(bool generic){std::vector<VkDescriptorSetLayoutBinding> bindings;for(unsigned i=0;i<(generic?30u:11u);i++){VkDescriptorSetLayoutBinding b{};b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;if(generic&&i==28)b.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;b.descriptorType=i<2 || (generic&&i==26)?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:generic?(i<18?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<26?VK_DESCRIPTOR_TYPE_SAMPLER:i==28?VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER):i<6?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<10?VK_DESCRIPTOR_TYPE_SAMPLER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings.push_back(b);}VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.bindingCount=bindings.size();ci.pBindings=bindings.data();VkDescriptorSetLayout out;vk(vkCreateDescriptorSetLayout(device,&ci,nullptr,&out));return out;}
 VkPipelineLayout pipeline_layout(VkDescriptorSetLayout set){VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};ci.setLayoutCount=1;ci.pSetLayouts=&set;VkPipelineLayout out;vk(vkCreatePipelineLayout(device,&ci,nullptr,&out));return out;}
 VkShaderModule module(const std::string& source,bool vertex){std::vector<uint32_t> spv;std::string error;if(!vkr::Compile(source,vertex,spv,&error))throw std::runtime_error(error);VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=spv.size()*4;ci.pCode=spv.data();VkShaderModule out;vk(vkCreateShaderModule(device,&ci,nullptr,&out));return out;}
 VkDescriptorSet descriptors(bool generic,const PixelKey& pixel,const Image& target,bool fetch){VkDescriptorSetLayout l=generic?universal_layout:special_layout;VkDescriptorSetAllocateInfo a{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};a.descriptorPool=descriptor_pool;a.descriptorSetCount=1;a.pSetLayouts=&l;VkDescriptorSet set;vk(vkAllocateDescriptorSets(device,&a,&set));std::array<VkWriteDescriptorSet,30> writes{};std::array<VkDescriptorImageInfo,30> image{};std::array<VkDescriptorBufferInfo,30> buffers{};for(unsigned i=0;i<(generic?30u:11u);i++){
 auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=set;w.dstBinding=i;w.descriptorCount=1;
 if(i<2 || (generic && i==26)){Buffer b=i==0?vc:i==1?dc:uc;buffers[i]={b.buffer,0,b.size};w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;w.pBufferInfo=&buffers[i];}
 else if((generic && i>=27 && i!=28)||(!generic&&i==10)){Buffer b=generic&&i==27?vertices:scene;buffers[i]={b.buffer,0,b.size};w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&buffers[i];}
 else if(generic&&i==28){image[i]={VK_NULL_HANDLE,target.view,VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR};w.descriptorType=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;w.pImageInfo=&image[i];}
 else if((generic&&i<18)||(!generic&&i<6)){Image im=tex2d;if(generic){if(i>=14)im=depth;else if(i>=10)im=cube;else if(i>=6)im=tex3d;}else{auto& t=pixel.tex[i-2];im=t.shadow?depth:t.kind==kTexCube?cube:t.kind==kTex3D?tex3d:tex2d;}image[i]={VK_NULL_HANDLE,im.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};w.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;w.pImageInfo=&image[i];}
 else {image[i]={generic?(i>=22?comparison:sampler):(pixel.tex[i-6].shadow?comparison:sampler),VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED};w.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLER;w.pImageInfo=&image[i];}
 }vkUpdateDescriptorSets(device,generic?30:11,writes.data(),0,nullptr);return set;}
 VkPipeline pipeline(bool generic,VkShaderModule vs,VkShaderModule ps,const Assembled& a,bool blend,bool fetch){
 VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[0].module=vs;stages[1].module=ps;stages[0].pName=stages[1].pName="main";
 VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};std::vector<VkVertexInputAttributeDescription> attributes;for(unsigned r=0;r<16;r++)if(a.regs&(1u<<r))attributes.push_back({r,0,VK_FORMAT_R32G32B32A32_SFLOAT,a.elements[r].offset});
 VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};if(!generic){vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=attributes.size();vi.pVertexAttributeDescriptions=attributes.data();}
 VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
 VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
 VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
 VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
 VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=fetch?15:write_mask;attachment.blendEnable=blend&&!fetch;attachment.srcColorBlendFactor=attachment.srcAlphaBlendFactor=src_factor;attachment.dstColorBlendFactor=attachment.dstAlphaBlendFactor=dst_factor;attachment.colorBlendOp=attachment.alphaBlendOp=blend_op;
 VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&attachment;cb.blendConstants[0]=.2f;cb.blendConstants[1]=.4f;cb.blendConstants[2]=.6f;cb.blendConstants[3]=.8f;cb.flags=fetch&&ordered?VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT:0;
 VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dy.dynamicStateCount=2;dy.pDynamicStates=dynamic;
 VkFormat format=VK_FORMAT_B8G8R8A8_UNORM;uint32_t index=0;VkRenderingInputAttachmentIndexInfoKHR input{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};input.colorAttachmentCount=1;input.pColorAttachmentInputIndices=&index;
 VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;rendering.pNext=fetch?&input:nullptr;
 VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.pNext=&rendering;ci.stageCount=2;ci.pStages=stages;ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pColorBlendState=&cb;ci.pDynamicState=&dy;ci.layout=generic?universal_pipeline_layout:special_pipeline_layout;VkPipeline p;vk(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&p));return p;}
 std::vector<uint8_t> draw(const State& s,const Assembled& a,const PixelKey& pixel,DrawConstants constants,bool generic,bool fetch,bool blend,unsigned repetitions=1){
  Image target=image(VK_IMAGE_TYPE_2D,VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_B8G8R8A8_UNORM,1,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  uint32_t flags=a.pretransformed?uPretransformed:0;if(s.vs&&!s.vs->program.empty())flags|=uProgrammableVertex;if(blend)flags|=uBlending;
  auto universal=MakeUniversalConstants(s,a,pixel,flags);uint64_t offsets[16]{};UniversalVertexOffsets(universal,a,offsets);universal[uBlend]=src_factor;universal[uBlend+1]=dst_factor;universal[uBlend+2]=blend_op;universal[uBlend+3]=write_mask;float bf[]={.2f,.4f,.6f,.8f};std::memcpy(&universal.words[uBlendConstant/4],bf,sizeof bf);
  std::memcpy(vc.mapped,s.vsc.data(),vc.size);std::memcpy(dc.mapped,&constants,sizeof constants);std::memcpy(uc.mapped,&universal,sizeof universal);
  VkShaderModule vs=generic?universal_vs:module(s.vs&&!s.vs->program.empty()?VertexProgramHlsl(s.vs->program,a.regs,a.decode):FixedVertexHlsl(a.regs,a.pretransformed,a.decode,MakeFixedKey(s)),true);
  VkShaderModule ps=generic?(fetch?fetch_ps:universal_ps):module(PixelHlsl(pixel),false);
  VkPipeline p;if(generic){auto& cached=universal_pipelines[fetch?1:0];if(!cached)cached=pipeline(true,vs,ps,a,blend,fetch);p=cached;}else p=pipeline(false,vs,ps,a,blend,fetch);auto set=descriptors(generic,pixel,target,fetch);
  begin();barrier(target,fetch?VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};color.imageView=target.view;color.imageLayout=target.layout;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.clearValue.color={{.05f,.1f,.15f,.2f}};
  VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.renderArea={{0,0},{W,H}};info.layerCount=1;info.colorAttachmentCount=1;info.pColorAttachments=&color;vkCmdBeginRendering(cmd,&info);
  if(fetch){uint32_t index=0;VkRenderingInputAttachmentIndexInfoKHR indices{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};indices.colorAttachmentCount=1;indices.pColorAttachmentInputIndices=&index;auto function=(PFN_vkCmdSetRenderingInputAttachmentIndicesKHR)vkGetDeviceProcAddr(device,"vkCmdSetRenderingInputAttachmentIndicesKHR");check(function!=nullptr,"local read command unavailable");function(cmd,&indices);}
  vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,generic?universal_pipeline_layout:special_pipeline_layout,0,1,&set,0,nullptr);
  VkViewport viewport{0,float(H),float(W),-float(H),0,1};VkRect2D scissor{{0,0},{W,H}};vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
  VkDeviceSize offset=0;if(!generic)vkCmdBindVertexBuffers(cmd,0,1,&vertices.buffer,&offset);
  vkCmdDraw(cmd,3*repetitions,1,0,0);vkCmdEndRendering(cmd);barrier(target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={W,H,1};vkCmdCopyImageToBuffer(cmd,target.image,target.layout,readback.buffer,1,&copy);submit();
  std::vector<uint8_t> result(W*H*4);std::memcpy(result.data(),readback.mapped,result.size());
  if(!generic)vkDestroyPipeline(device,p,nullptr);vk(vkFreeDescriptorSets(device,descriptor_pool,1,&set));if(!generic){vkDestroyShaderModule(device,vs,nullptr);vkDestroyShaderModule(device,ps,nullptr);}return result;
 }
};
static void compare(const std::vector<uint8_t>& a,const std::vector<uint8_t>& b,const char* label){unsigned mismatch=0,maximum=0;for(size_t i=0;i<a.size();i++){unsigned error=unsigned(std::abs(int(a[i])-int(b[i])));maximum=std::max(maximum,error);if(error>1)mismatch++;}if(mismatch)throw std::runtime_error(std::string(label)+": "+std::to_string(mismatch)+" mismatched channels, max error "+std::to_string(maximum));std::cout<<"PASS: "<<label<<std::endl;}
int main(){try{
 GPU gpu;State state;state.vs_handle=0x44;state.tss[0][kTssColorOp]=2;state.tss[0][kTssColorArg1]=0;state.tss[0][kTssAlphaOp]=2;state.tss[0][kTssAlphaArg1]=0;state.tss[1][kTssColorOp]=1;
 Vertex triangle[12]{};float positions[3][4]={{2,2,.4f,1},{44,2,.4f,1},{2,44,.4f,1}};
 for(unsigned i=0;i<12;i++){std::copy(positions[i%3],positions[i%3]+4,triangle[i].position);float color[]={.21f,.37f,.53f,.61f};std::copy(color,color+4,triangle[i].color);triangle[i].uv[0]=.25f;triangle[i].uv[1]=.75f;triangle[i].uv[3]=1;triangle[i].normal[2]=1;triangle[i].weights[0]=1;}
 std::memcpy(gpu.vertices.mapped,triangle,sizeof triangle);
 Assembled a;a.pretransformed=true;a.regs=(1<<0)|(1<<3)|(1<<9)|(1<<2)|(1<<1);a.buffer_count=1;a.buffers[0]={reinterpret_cast<const uint8_t*>(triangle),sizeof triangle,sizeof(Vertex)};a.elements[0]={2,0,0};a.elements[3]={2,0,16};a.elements[9]={2,0,32};a.elements[2]={2,0,48};a.elements[1]={2,0,64};
 DrawConstants dc{};dc.surf[0]=GPU::W;dc.surf[1]=GPU::H;dc.surf[2]=1.f/16777215;dc.surf[3]=1;dc.misc[1]=7;dc.misc[2]=3;dc.misc[3]=1;for(auto& size:dc.texsize){size[0]=size[1]=GPU::W;}dc.tfactor[0]=.7f;dc.tfactor[1]=.3f;dc.tfactor[2]=.2f;dc.tfactor[3]=.4f;
 TexInfo textures[4]{};auto key=MakePixelKey(state,textures);
 auto reference=gpu.draw(state,a,key,dc,false,false,false);auto universal=gpu.draw(state,a,key,dc,true,false,false);compare(reference,universal,"universal pretransformed diffuse");
 auto fetch=gpu.draw(state,a,key,dc,true,true,false);compare(reference,fetch,"framebuffer fetch opaque");
 auto additive_reference=gpu.draw(state,a,key,dc,false,false,true,2);auto additive_generic=gpu.draw(state,a,key,dc,true,true,true,2);compare(additive_reference,additive_generic,"overlapping primitives programmable blend");
 unsigned cases=3;
 textures[0].kind=kTex2D;
 for(unsigned op=2;op<=24;op++){
  state.tss[0][kTssColorOp]=op;state.tss[0][kTssColorArg1]=0;state.tss[0][kTssColorArg2]=2;state.tss[0][kTssColorArg0]=3;
  state.tss[0][kTssAlphaOp]=op;state.tss[0][kTssAlphaArg1]=0;state.tss[0][kTssAlphaArg2]=2;state.tss[0][kTssAlphaArg0]=3;
  key=MakePixelKey(state,textures);reference=gpu.draw(state,a,key,dc,false,false,false);universal=gpu.draw(state,a,key,dc,true,false,false);compare(reference,universal,("texture combiner "+std::to_string(op)).c_str());cases++;
 }

 // Generated legal register-combiner states, independent of gameplay recipes.
 uint32_t random=0x4e474255;
 auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
 for(unsigned trial=0;trial<96;trial++){
  PixelKey synthetic{};synthetic.programmable=1;synthetic.count=1+next()%8;
  for(unsigned stage=0;stage<(synthetic.count&255);stage++){
   auto operand=[&](){const unsigned regs[]={0,1,2,3,4,5,8,9,10,11,12,13};return regs[next()%12]|((next()%2)<<4)|((next()%8)<<5);};
   auto inputs=[&](){uint32_t word=0;for(unsigned i=0;i<4;i++)word=(word<<8)|operand();return word;};
   auto outputs=[&](){const unsigned regs[]={0,4,5,8,9,10,11,12,13};return regs[next()%9]|(regs[next()%9]<<4)|(regs[next()%9]<<8)|((next()%64)<<12);};
   synthetic.rgb_in[stage]=inputs();synthetic.a_in[stage]=inputs();synthetic.rgb_out[stage]=outputs();synthetic.a_out[stage]=outputs();
  }
  if(trial&1){synthetic.final_abcd=0x04200c05;synthetic.final_efg=0x01021c00|((trial&7)<<5);}
  for(auto& constant:dc.pc)for(float& lane:constant)lane=float(next()%256)/255;
  reference=gpu.draw(state,a,synthetic,dc,false,false,false);universal=gpu.draw(state,a,synthetic,dc,true,false,false);
  compare(reference,universal,("register combiners "+std::to_string(trial)).c_str());cases++;
 }

 key=MakePixelKey(state,textures);
 for(unsigned trial=0;trial<80;trial++){
  gpu.src_factor=VkBlendFactor(next()%15);gpu.dst_factor=VkBlendFactor(next()%15);gpu.blend_op=VkBlendOp(next()%5);gpu.write_mask=next()%16;
  reference=gpu.draw(state,a,key,dc,false,false,true,2);universal=gpu.draw(state,a,key,dc,true,true,true,2);
  compare(reference,universal,("blend equation and write mask "+std::to_string(trial)).c_str());cases++;
 }
 gpu.src_factor=gpu.dst_factor=VK_BLEND_FACTOR_ONE;gpu.blend_op=VK_BLEND_OP_ADD;gpu.write_mask=15;
 // Small NV2A programs exercise the same token decoder used by specialization.
 auto program=std::make_shared<VertexShader>();state.vs=program;state.vs_handle=1;
 auto move=[](unsigned input,unsigned output,bool last){return std::array<uint32_t,4>{0,(1u<<21)|(input<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(output<<3)|unsigned(last)};};
 for(auto instruction:{move(0,0,false),move(3,3,false),move(9,9,true)})program->program.insert(program->program.end(),instruction.begin(),instruction.end());
 key=MakePixelKey(state,textures);reference=gpu.draw(state,a,key,dc,false,false,false);universal=gpu.draw(state,a,key,dc,true,false,false);compare(reference,universal,"NV2A vertex instruction interpretation");cases++;
 state.vs=nullptr;state.vs_handle=0x44;
 check(gpu.validation_errors==0,"Vulkan validation reported errors");
 std::cout<<"PASS: "<<cases<<" uncached synthetic GPU image comparisons, including same-draw overlapping blend primitives\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
