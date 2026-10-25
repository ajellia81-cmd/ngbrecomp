// Included inside the Vulkan backend's namespace. Every recipe owns its CPU
// inputs and pins immutable texture versions until EndFrame. The authoritative
// guest frame is rendered first; replay never runs visibility queries or guest
// code, and restores all guest-visible render targets after drawing the midpoint.
#ifdef NGB_SMOOTH_MOTION
static int64_t MotionClockNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void VulkanBackend::MotionCopyImage(Image& from,Image& to) {
    EndRendering();
    Barrier(from,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Barrier(to,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy regions[2]{};
    uint32_t count=from.depth ? 2 : 1;
    for (uint32_t i=0;i<count;i++) {
        regions[i].srcSubresource=regions[i].dstSubresource={
            from.depth ? (i ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT) : VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        regions[i].extent={from.w,from.h,1};
    }
    d_.vkCmdCopyImage(cmd_,from.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,to.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,count,regions);
}

void VulkanBackend::MotionTouch(Target *target) {
    if (!target || !motion_capture_ || !motion_valid_) return;
    if (target->samples!=1) { motion_valid_=false; return; }
    auto& saved=motion_targets_[target->img.image];
    if (saved.seen==frame_ && saved.initial.image) return;
    if (!saved.initial.image) {
        uint64_t bytes=uint64_t(target->img.w)*target->img.h*(target->img.depth ? 8 : 4)*2;
        if (motion_gpu_bytes_+bytes>256u*1024u*1024u) { motion_valid_=false; return; }
        for (Image *image : {&saved.initial,&saved.final})
            if (!CreateImage(*image,target->img.format,target->img.w,target->img.h,1,1,target->img.depth,false,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
                motion_valid_=false; return;
            }
        motion_gpu_bytes_+=bytes;
    }
    saved.source=&target->img; saved.seen=frame_;
    MotionCopyImage(target->img,saved.initial);
}

void VulkanBackend::MotionReplay(const MotionDraw& p) {
    // Upload can submit/reset the ring. All following bindings are rebuilt from
    // this owned packet, exactly as the original draw rebuilds its bindings.
    uint64_t required=sizeof p.vc+sizeof p.dc+ringAlign_*2+16*16+4+p.indices.size();
    for (uint32_t i=0;i<p.buffers;i++) required+=p.vertices[i].size();
    if (ring_pos_+required>kRingSize) Submit();
    VkDeviceSize vc=Upload(p.vc.data(),sizeof p.vc,ringAlign_);
    VkDeviceSize dc=Upload(&p.dc,sizeof p.dc,ringAlign_);
    VkDeviceSize offsets[16]{}; VkBuffer buffers[16]{};
    for (uint32_t i=0;i<p.buffers;i++) {
        offsets[i]=Upload(p.vertices[i].data(),p.vertices[i].size(),16); buffers[i]=ring_;
    }
    VkDeviceSize indices=p.indices.empty() ? 0 : Upload(p.indices.data(),p.indices.size(),4);
    for (size_t i=0;i<4;i++) if (p.target_images[i])
        Barrier(*p.target_images[i],p.images[i].imageLayout);
    BeginRendering(p.rt,p.ds);
    d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline);
    VkDescriptorBufferInfo uniform[2]={{ring_,vc,sizeof p.vc},{ring_,dc,sizeof p.dc}};
    VkWriteDescriptorSet writes[10]{};
    for (uint32_t i=0;i<10;i++) {
        auto& w=writes[i]; w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstBinding=i; w.descriptorCount=1;
        if (i<2) { w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.pBufferInfo=&uniform[i]; }
        else { w.descriptorType=i<6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
            w.pImageInfo=i<6 ? &p.images[i-2] : &p.samplers[i-6]; }
    }
    d_.vkCmdPushDescriptorSetKHR(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe_layout_,0,10,writes);
    d_.vkCmdSetViewport(cmd_,0,1,&p.viewport); d_.vkCmdSetScissor(cmd_,0,1,&p.scissor);
    d_.vkCmdSetBlendConstants(cmd_,p.blend);
    d_.vkCmdSetStencilReference(cmd_,VK_STENCIL_FACE_FRONT_AND_BACK,p.stencil);
    if (p.lines) d_.vkCmdSetLineWidth(cmd_,p.line);
    if (p.buffers) d_.vkCmdBindVertexBuffers(cmd_,0,p.buffers,buffers,offsets);
    if (p.index_count) {
        d_.vkCmdBindIndexBuffer(cmd_,ring_,indices,p.index16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        d_.vkCmdDrawIndexed(cmd_,p.index_count,1,0,p.base_vertex,0);
    } else d_.vkCmdDraw(cmd_,p.draw_count,1,0,0);
}

void VulkanBackend::MotionPresent(const Surface& front,Image& endpoint,float bars,float bars_y) {
    int64_t now=MotionClockNs();
#ifdef NGB_IOS_APP
    if (motion_epoch_!=ngb_ios_motion_epoch() || ngb_ios_display_hz()<100 || !ngb_pc_setting(26)) return;
#endif
    if (!motion_capture_ || !motion_valid_ || !motion_changed_ || !motion_3d_ ||
        ngb_quick_wanted() || !motion_last_ns_ || now-motion_last_ns_>50000000LL) return;
    Target *target=EnsureTarget(front,false);
    if (!target || !motion_valid_) return;
    int slot=-1;
    for (int i=0;i<3;i++) {
        int candidate=(motion_display_next_+i)%3;
        if (!nv2a_present_external_image_busy(motion_display_[candidate].image)) { slot=candidate; break; }
    }
    if (slot<0) return;
    Image& midpoint_image=motion_display_[slot]; motion_display_next_=(slot+1)%3;
    // The snapshots preserve both the frame's initial inputs and its final
    // authoritative results. Shadows, copies and depth clears replay in order.
    EndRendering();
    for (auto& [key,saved]:motion_targets_) if (saved.seen==frame_)
        MotionCopyImage(*saved.source,saved.final);
    Submit();
    for (auto& [key,saved]:motion_targets_) if (saved.seen==frame_)
        MotionCopyImage(saved.initial,*saved.source);
    for (auto& command:motion_commands_) command();
    if (midpoint_image.image && (midpoint_image.w!=target->img.w || midpoint_image.h!=target->img.h)) {
        Submit(); nv2a_vk_queue_lock(); d_.vkDeviceWaitIdle(dev_); nv2a_vk_queue_unlock();
        DestroyImage(midpoint_image);
    }
    if (!midpoint_image.image) CreateImage(midpoint_image,kColorFormat,target->img.w,target->img.h,1,1,false,false,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (midpoint_image.image) {
        if (gamma_on_ || bars>0 || bars_y>0) GammaCopy(target->img,midpoint_image,bars,bars_y);
        else MotionCopyImage(target->img,midpoint_image);
        Barrier(midpoint_image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    Submit();
    // BeginRendering updates target bookkeeping, but replay uses single-sample
    // images only. Restore authoritative pixels/depth before returning to guest.
    for (auto& [key,saved]:motion_targets_) if (saved.seen==frame_)
        MotionCopyImage(saved.final,*saved.source);
    Submit();
    // Display cadence belongs to the presenter worker. Sleeping/acquiring twice
    // inside Xbox Swap made gameplay miss every other 60 Hz vblank.
    if (midpoint_image.image && nv2a_present_queue_pair(midpoint_image.image,endpoint.image,endpoint.w,endpoint.h)) {
        motion_pictures_++; motion_endpoint_=true;
    }

}

void VulkanBackend::MotionFinish() {
    int64_t now=MotionClockNs();
    double hz=0;
#ifdef NGB_IOS_APP
    hz=ngb_ios_display_hz();
#endif
    if (now-motion_log_ns_>=5000000000LL) {
        if (motion_log_ns_) std::fprintf(stderr,
            "[Smooth Motion] midpoint pairs queued %u/5s; eligible 3D draws %u, changed matches %u; packet %.1f MiB; snapshots %.1f MiB; valid %d, MSAA %u, custom glow %d, display %.1f Hz; guest clock 60 Hz\n",
            motion_pictures_,motion_3d_,motion_changed_,double(motion_cpu_bytes_)/1048576,double(motion_gpu_bytes_)/1048576,int(motion_valid_),msaa_,int(ContinuousGlowEnabled() || GlowSoftenMode()!=0),hz);
        uint64_t mids,ends,replaced; nv2a_present_pair_counts(&mids,&ends,&replaced);
        std::fprintf(stderr,"[Smooth Motion presenter] cumulative midpoint %llu, endpoint %llu, superseded pairs %llu\n",
            (unsigned long long)mids,(unsigned long long)ends,(unsigned long long)replaced);
        motion_log_ns_=now; motion_pictures_=0;
    }
    bool wanted=false;
#ifdef NGB_IOS_APP
    wanted=shared_ && ngb_pc_setting(26) && ngb_ios_display_hz()>=100;
#else
    const char *env=std::getenv("NGB_SMOOTH_MOTION"); wanted=shared_ && env && std::strcmp(env,"0");
#endif
    // Custom glow/MSAA paths need their own replay recipes. Keep their original
    // rendering intact rather than interpolate an incomplete graph.
    wanted=wanted && !ContinuousGlowEnabled() && GlowSoftenMode()==0 && msaa_==1 && threed_>0 && !ngb_quick_wanted();
    bool reset=!motion_valid_ || !wanted || (motion_last_ns_ && now-motion_last_ns_>50000000LL);
#ifdef NGB_IOS_APP
    reset=reset || motion_epoch_!=ngb_ios_motion_epoch(); motion_epoch_=ngb_ios_motion_epoch();
#endif
    if (reset) { motion_history_.Reset(); nv2a_present_cancel_pairs(); }
    else motion_history_.Finish();
    bool prune=reset;
    for (auto& [key,saved]:motion_targets_) if (frame_-saved.seen>2) prune=true;
    if (prune && !motion_targets_.empty()) {
        Submit();
        for (auto it=motion_targets_.begin();it!=motion_targets_.end();) {
            if (reset || frame_-it->second.seen>2) {
                auto& p=it->second;
                motion_gpu_bytes_-=p.initial.image ? uint64_t(p.initial.w)*p.initial.h*(p.initial.depth ? 8 : 4)*2 : 0;
                DestroyImage(p.initial); DestroyImage(p.final); it=motion_targets_.erase(it);
            } else ++it;
        }
        if (motion_targets_.empty()) motion_gpu_bytes_=0;
    }
    motion_commands_.clear(); motion_cpu_bytes_=0; motion_3d_=motion_changed_=0;
    motion_capture_=motion_valid_=wanted; motion_last_ns_=now;
}
#endif
