// Experimental complete pipeline bank. Never create one from a draw callback.
static uint32_t UniversalFamily(const PipeKey& k) {
    return (k.samples?k.samples:1)<<8 | (k.has_rt?1:0)<<6 | (k.has_ds?1:0)<<5 |
        (k.topo==2?(k.fill==0x1B01?1:k.fill==0x1B00?2:0):0)<<2 | k.topo;
}
VkPipeline VulkanBackend::MakeUniversalPipeline(const PipeKey& k, VkShaderModule vs, VkShaderModule ps) {
    VkPipeline pipe = VK_NULL_HANDLE;
    if (vs && ps) {
        VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                 {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        st[0].module = vs;
        st[0].pName = "main";
        st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        st[1].module = ps;
        st[1].pName = "main";
        VkVertexInputBindingDescription vb[16];
        VkVertexInputAttributeDescription va[16];
        uint32_t nb = 0, na = 0;
        bool used[16] = {};
        for (uint32_t r = 0; r < 16; r++) {
            if (!((k.regs >> r) & 1)) continue;
            uint32_t e = k.elements[r];
            uint32_t buf = (e >> 8) & 0xFF;
            va[na++] = {r, buf, VertexFormat(uint8_t(e & 0xFF)), e >> 16};
            used[buf] = true;
        }
        for (uint32_t b = 0; b < 16; b++)
            if (used[b]) vb[nb++] = {b, k.strides[b], VK_VERTEX_INPUT_RATE_VERTEX};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = 0;
        vi.pVertexBindingDescriptions = vb;
        vi.vertexAttributeDescriptionCount = 0;
        vi.pVertexAttributeDescriptions = va;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = k.topo == 0 ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                      : k.topo == 1 ? (k.strip ? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
                      : (k.strip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1;
        vp.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = k.fill == 0x1B01 ? VK_POLYGON_MODE_LINE : k.fill == 0x1B00 ? VK_POLYGON_MODE_POINT : VK_POLYGON_MODE_FILL;
        rs.cullMode = !k.cull ? VK_CULL_MODE_NONE : k.cullfront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
        // Direct3D's winding is in its y-down screen space, Vulkan's in the framebuffer after the
        // viewport transform: with the flipped viewport the two agree, so no inversion
        rs.frontFace = k.ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        rs.depthBiasEnable = VK_TRUE;
        rs.depthBiasConstantFactor = float(k.zbias);
        rs.depthBiasSlopeFactor = k.zslope;
        rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VkSampleCountFlagBits(k.samples ? k.samples : 1);
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = k.zen;
        ds.depthWriteEnable = k.zen && k.zwrite;
        ds.depthCompareOp = k.zen ? Cmp(k.zfunc) : VK_COMPARE_OP_ALWAYS;
        ds.stencilTestEnable = k.sten;
        VkStencilOpState so{StencilOp(k.sfail), StencilOp(k.spass), StencilOp(k.szfail), Cmp(k.sfunc), k.smask, k.swmask, 0};
        ds.front = ds.back = so;
        VkPipelineColorBlendAttachmentState cb{};
        cb.blendEnable = VK_FALSE;
        cb.srcColorBlendFactor = Blend(k.src, false);
        cb.dstColorBlendFactor = Blend(k.dst, false);
        cb.colorBlendOp = BlendOp(k.op);
        cb.srcAlphaBlendFactor = Blend(k.src, true);
        cb.dstAlphaBlendFactor = Blend(k.dst, true);
        cb.alphaBlendOp = BlendOp(k.op);
        cb.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbs.attachmentCount = k.has_rt ? 1 : 0;
        cbs.pAttachments = &cb;
        // the line width only on the line pipelines: the others, nearly all, stay as the caches have them
        VkDynamicState dyn[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,VK_DYNAMIC_STATE_LINE_WIDTH,VK_DYNAMIC_STATE_DEPTH_BIAS,
            VK_DYNAMIC_STATE_CULL_MODE_EXT,VK_DYNAMIC_STATE_FRONT_FACE_EXT,VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,VK_DYNAMIC_STATE_STENCIL_OP_EXT,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,VK_DYNAMIC_STATE_STENCIL_WRITE_MASK};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dy.dynamicStateCount=sizeof(dyn)/sizeof(dyn[0]);dy.pDynamicStates=dyn;
        if(universal_ordered_ && k.has_rt)cbs.flags=VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
        VkFormat cf = kColorFormat;
        VkPipelineRenderingCreateInfo pr{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        pr.colorAttachmentCount = k.has_rt ? 1 : 0;
        pr.pColorAttachmentFormats = &cf;
        pr.depthAttachmentFormat = k.has_ds ? ds_format_ : VK_FORMAT_UNDEFINED;
        pr.stencilAttachmentFormat = k.has_ds ? ds_format_ : VK_FORMAT_UNDEFINED;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        ci.pNext = &pr;
        ci.stageCount = 2;
        ci.pStages = st;
        ci.pVertexInputState = &vi;
        ci.pInputAssemblyState = &ia;
        ci.pViewportState = &vp;
        ci.pRasterizationState = &rs;
        ci.pMultisampleState = &ms;
        ci.pDepthStencilState = &ds;
        ci.pColorBlendState = &cbs;
        ci.pDynamicState = &dy;
        ci.layout = universal_layout_;
        uint32_t index=0;VkRenderingInputAttachmentIndexInfoKHR local{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};
        local.colorAttachmentCount=k.has_rt?1:0;local.pColorAttachmentInputIndices=&index;pr.pNext=&local;
        if (d_.vkCreateGraphicsPipelines(dev_, pcache_, 1, &ci, nullptr, &pipe) != VK_SUCCESS) pipe = VK_NULL_HANDLE;
    }
    return pipe;
}


bool VulkanBackend::UniversalInit() {
    if(!universal_capable_)return false;
    VkPhysicalDevicePushDescriptorPropertiesKHR push{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&push;
    vkGetPhysicalDeviceProperties2(phys_,&properties);
    if(push.maxPushDescriptors<30)return false;
    VkDescriptorSetLayoutBinding bindings[30]{};
    for(uint32_t i=0;i<30;i++) {
        auto& b=bindings[i];b.binding=i;b.descriptorCount=1;
        b.stageFlags=i==28?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
        b.descriptorType=i<2 || i==26?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            i<18?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<26?VK_DESCRIPTOR_TYPE_SAMPLER:
            i==28?VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    set.flags=VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;set.bindingCount=30;set.pBindings=bindings;
    if(d_.vkCreateDescriptorSetLayout(dev_,&set,nullptr,&universal_set_)!=VK_SUCCESS)return false;
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.setLayoutCount=1;layout.pSetLayouts=&universal_set_;
    if(d_.vkCreatePipelineLayout(dev_,&layout,nullptr,&universal_layout_)!=VK_SUCCESS)return false;
    auto module=[&](const std::string& source,bool vertex){std::vector<uint32_t> words;return Compile(source,vertex,words)?MakeModule(words):VK_NULL_HANDLE;};
    universal_vs_=module(UniversalVertexHlsl(),true);
    universal_ps_[0]=module(UniversalPixelHlsl(false),false);
    universal_ps_[1]=module(UniversalPixelHlsl(true),false);
    universal_ps_[2]=module(UniversalPixelHlsl(false,true),false);
    if(!universal_vs_ || !universal_ps_[0] || !universal_ps_[1] || !universal_ps_[2])return false;
    for(uint32_t samples=1;samples<=64;samples*=2)if(ms_counts_&samples)
        for(uint32_t targets=1;targets<4;targets++)for(uint32_t topology=0;topology<3;topology++)
            for(uint32_t fill=0;fill<3;fill++) {
                if(topology<2 && fill>0)continue;
#ifdef __APPLE__
                // Metal portability does not expose polygonMode POINT.
                if(fill==2)continue;
#endif
                PipeKey key{};key.samples=samples;key.topo=topology;key.has_rt=targets&1;key.has_ds=(targets>>1)&1;
                key.fill=fill==1?0x1B01:fill==2?0x1B00:0x1B02;key.wmask=15;
                auto pipeline=MakeUniversalPipeline(key,universal_vs_,universal_ps_[key.has_rt?(samples>1?1:0):2]);
                if(!pipeline)return false;
                universal_pipes_.emplace(UniversalFamily(key),pipeline);
            }
    auto usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(!CreateImage(universal_null3d_,kColorFormat,1,1,1,1,false,false,usage,1,false,true) ||
       !CreateImage(universal_depth_,ds_format_,1,1,1,1,true,false,usage))return false;
    for(auto* image:{&null2d_,&nullcube_,&universal_null3d_}) {
        Barrier(*image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clear{};VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,image==&nullcube_?6u:1u};
        d_.vkCmdClearColorImage(cmd_,image->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
        Barrier(*image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    Barrier(universal_depth_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearDepthStencilValue clear{1,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
    d_.vkCmdClearDepthStencilImage(cmd_,universal_depth_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
    Barrier(universal_null3d_,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Barrier(universal_depth_,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(d_.vkCreateSampler(dev_,&sampler,nullptr,&universal_normal_)!=VK_SUCCESS)return false;
    sampler.compareEnable=VK_TRUE;sampler.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
    if(d_.vkCreateSampler(dev_,&sampler,nullptr,&universal_comparison_)!=VK_SUCCESS)return false;
    if(!compiler_.joinable())compiler_=std::thread(&VulkanBackend::Compiler,this);
    std::fprintf(stderr,"[Universal shaders] ready: %zu complete pipeline families; ordered-access extension %u; no gameplay recipes required\n",universal_pipes_.size(),unsigned(universal_ordered_));
    universal_ready_=true;return true;
}
void VulkanBackend::UniversalDestroy() {
    for(auto& item:universal_pipes_)d_.vkDestroyPipeline(dev_,item.second,nullptr);
    universal_pipes_.clear();
    if(universal_vs_)d_.vkDestroyShaderModule(dev_,universal_vs_,nullptr);
    for(auto shader:universal_ps_)if(shader)d_.vkDestroyShaderModule(dev_,shader,nullptr);
    if(universal_layout_)d_.vkDestroyPipelineLayout(dev_,universal_layout_,nullptr);
    if(universal_set_)d_.vkDestroyDescriptorSetLayout(dev_,universal_set_,nullptr);
    if(universal_normal_)d_.vkDestroySampler(dev_,universal_normal_,nullptr);
    if(universal_comparison_)d_.vkDestroySampler(dev_,universal_comparison_,nullptr);
    DestroyImage(universal_null3d_);DestroyImage(universal_depth_);
    universal_ready_=false;
}
VkShaderModule VulkanBackend::RequestModule(uint64_t key,std::function<std::string()> factory,bool vertex) {
    {
        std::unique_lock<std::recursive_mutex> lock(cache_mu_,std::try_to_lock);
        if(!lock.owns_lock())return VK_NULL_HANDLE;
        auto found=modules_.find(key);if(found!=modules_.end())return found->second;
    }
    std::lock_guard<std::mutex> lock(job_mu_);
    if(!pending_mods_.count(key)) {
        pending_mods_[key]=true;Job job{false,key,{},vertex,{}};job.factory=std::move(factory);
        jobs_.push_back(std::move(job));job_cv_.notify_one();
    }
    return VK_NULL_HANDLE;
}
VkPipeline VulkanBackend::RequestPipeline(const PipeKey& key) {
    {
        std::unique_lock<std::recursive_mutex> lock(cache_mu_,std::try_to_lock);
        if(!lock.owns_lock())return VK_NULL_HANDLE;
        auto found=pipelines_.find(key);if(found!=pipelines_.end())return found->second;
        auto vs=modules_.find(key.vs),ps=modules_.find(key.ps);
        if(vs==modules_.end() || ps==modules_.end() || !vs->second || !ps->second)return VK_NULL_HANDLE;
    }
    std::lock_guard<std::mutex> lock(job_mu_);
    if(!pending_pipes_.count(key)) {
        pending_pipes_[key]=true;jobs_.push_back({true,0,{},false,key});job_cv_.notify_one();
    }
    return VK_NULL_HANDLE;
}

void VulkanBackend::UniversalDynamic(const PipeKey& k,float bias,float slope) {
    d_.vkCmdSetCullModeEXT(cmd_,!k.cull?VK_CULL_MODE_NONE:k.cullfront?VK_CULL_MODE_FRONT_BIT:VK_CULL_MODE_BACK_BIT);
    d_.vkCmdSetFrontFaceEXT(cmd_,k.ccw?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE);
    d_.vkCmdSetPrimitiveTopologyEXT(cmd_,k.topo==0?VK_PRIMITIVE_TOPOLOGY_POINT_LIST:k.topo==1?
        (k.strip?VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:VK_PRIMITIVE_TOPOLOGY_LINE_LIST):
        (k.strip?VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
    d_.vkCmdSetDepthTestEnableEXT(cmd_,k.zen);d_.vkCmdSetDepthWriteEnableEXT(cmd_,k.zen && k.zwrite);
    d_.vkCmdSetDepthCompareOpEXT(cmd_,k.zen?Cmp(k.zfunc):VK_COMPARE_OP_ALWAYS);
    d_.vkCmdSetStencilTestEnableEXT(cmd_,k.sten);
    d_.vkCmdSetStencilOpEXT(cmd_,VK_STENCIL_FACE_FRONT_AND_BACK,StencilOp(k.sfail),StencilOp(k.spass),StencilOp(k.szfail),Cmp(k.sfunc));
    d_.vkCmdSetStencilCompareMask(cmd_,VK_STENCIL_FACE_FRONT_AND_BACK,k.smask);
    d_.vkCmdSetStencilWriteMask(cmd_,VK_STENCIL_FACE_FRONT_AND_BACK,k.swmask);
    d_.vkCmdSetDepthBias(cmd_,bias,0,slope);
    // Every generic pipeline declares line width dynamic, including triangles.
    d_.vkCmdSetLineWidth(cmd_,1);
}
void VulkanBackend::UniversalBindings(const TexInfo* info,const VkDescriptorImageInfo* images,const VkDescriptorImageInfo* samplers,
    Target* rt,VkDeviceSize vc,VkDeviceSize dc,VkDeviceSize uc,const VkDescriptorBufferInfo& scene) {
    VkDescriptorBufferInfo buffers[30]{};VkDescriptorImageInfo bindings[30]{};VkWriteDescriptorSet writes[30]{};
    buffers[0]={ring_,vc,192*16};buffers[1]={ring_,dc,sizeof(DrawConstants)};buffers[26]={ring_,uc,sizeof(UniversalConstants)};
    buffers[27]={ring_,0,kRingSize};buffers[29]=scene;
    for(uint32_t stage=0;stage<4;stage++) {
        bindings[2+stage]={VK_NULL_HANDLE,null2d_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        bindings[6+stage]={VK_NULL_HANDLE,universal_null3d_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        bindings[10+stage]={VK_NULL_HANDLE,nullcube_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        bindings[14+stage]={VK_NULL_HANDLE,universal_depth_.view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        uint32_t actual=info[stage].shadow?14:info[stage].kind==kTexCube?10:info[stage].kind==kTex3D?6:2;
        if(images[stage].imageView && images[stage].imageView!=null2d_.view && images[stage].imageView!=nullcube_.view)bindings[actual+stage]=images[stage];
        bindings[18+stage]={universal_normal_,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED};
        bindings[22+stage]={universal_comparison_,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED};
        bindings[(info[stage].shadow?22:18)+stage]=samplers[stage];
    }
    bindings[28]={VK_NULL_HANDLE,rt?(rt->samples>1?rt->ms.view:rt->img.view):null2d_.view,
        rt?VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    for(uint32_t i=0;i<30;i++) {
        auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstBinding=i;w.descriptorCount=1;
        if(i<2 || i==26){w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;w.pBufferInfo=&buffers[i];}
        else if(i==27 || i==29){w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&buffers[i];}
        else {w.descriptorType=i==28?VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:i<18?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:VK_DESCRIPTOR_TYPE_SAMPLER;w.pImageInfo=&bindings[i];}
    }
    d_.vkCmdPushDescriptorSetKHR(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,universal_layout_,0,30,writes);
    if(rt) {
        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barrier.srcStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;barrier.srcAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;barrier.dstAccessMask=VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;
        dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&barrier;d_.vkCmdPipelineBarrier2(cmd_,&dependency);
    }
}
