// Included inside the Vulkan backend's namespace. Every recipe owns its CPU
// inputs and pins immutable texture versions until EndFrame. The authoritative
// guest frame is rendered first; replay never runs visibility queries or guest
// code. Midpoint rendering writes isolated targets, never guest-visible pixels.
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
    if (saved.seen==frame_ && saved.replay.img.image) return;
    if (!saved.replay.img.image) {
        uint64_t bytes=uint64_t(target->img.w)*target->img.h*(target->img.depth ? 8 : 4);
        if (motion_gpu_bytes_+bytes>256u*1024u*1024u) { motion_valid_=false; return; }
        auto& clone=saved.replay; clone.gw=target->gw; clone.gh=target->gh; clone.samples=1;
        if (!CreateImage(clone.img,target->img.format,target->img.w,target->img.h,1,1,target->img.depth,false,
            VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|
            (target->img.depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))) {
            motion_valid_=false; return;
        }
        motion_gpu_bytes_+=bytes;
    }
    saved.source=target; saved.seen=frame_;
    // Snapshot directly into the isolated replay target before the guest writes
    // it. Replay never overwrites authoritative guest pixels or query results.
    // A new target has not been cleared yet. Copying its undefined depth and
    // then LOADing it would make replay fail depth tests from its first draw.
    if (target->drawn) MotionCopyImage(target->img,saved.replay.img);
    saved.replay.drawn=target->drawn;
}
Target *VulkanBackend::MotionTargetFor(Target *target) {
    if (!target) return nullptr;
    return &motion_targets_.at(target->img.image).replay;
}
Image *VulkanBackend::MotionImageFor(Image *image) {
    for (auto& [key,saved]:motion_targets_) {
        if (&saved.source->img==image) return &saved.replay.img;
        if (&saved.source->fb==image) return &saved.replay.fb;
    }
    return nullptr;
}
void VulkanBackend::MotionFeedback(Target *target) {
    Target *clone=MotionTargetFor(target);
    if (!clone->fb.image) {
        uint64_t bytes=uint64_t(clone->img.w)*clone->img.h*4;
        if (motion_gpu_bytes_+bytes>256u*1024u*1024u) { motion_valid_=false; return; }
        if (!CreateImage(clone->fb,clone->img.format,clone->img.w,clone->img.h,1,1,false,false,
            VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT)) { motion_valid_=false; return; }
        motion_gpu_bytes_+=uint64_t(clone->fb.w)*clone->fb.h*4;
    }
    MotionCopyImage(clone->img,clone->fb);
    Barrier(clone->fb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanBackend::MotionReplay(const MotionDraw& p) {
    // Upload can submit/reset the ring. All following bindings are rebuilt from
    // this owned packet, exactly as the original draw rebuilds its bindings.
    uint64_t required=sizeof p.vc+sizeof p.dc+ringAlign_*3+16*16+4+p.indices.size()+p.scene.positions.size()*sizeof(ScenePosition);
    for (uint32_t i=0;i<p.buffers;i++) required+=p.vertices[i].size();
    if (required>kUploadSlotBytes) { motion_valid_=false; return; }
    if (!UploadRingFits(ring_pos_,required)) Submit();
    VkDeviceSize vc=Upload(p.vc.data(),sizeof p.vc,ringAlign_);
    DrawConstants constants=p.dc;
    constants.motion[0]=p.scene.positions.empty()?0.f:1.f;
    VkDeviceSize dc=Upload(&constants,sizeof constants,ringAlign_);
    VkDeviceSize positions=p.scene.positions.empty()?vc:Upload(p.scene.positions.data(),p.scene.positions.size()*sizeof(ScenePosition),ringAlign_);
    VkDeviceSize offsets[16]{}; VkBuffer buffers[16]{};
    for (uint32_t i=0;i<p.buffers;i++) {
        offsets[i]=Upload(p.vertices[i].data(),p.vertices[i].size(),16); buffers[i]=ring_;
    }
    VkDeviceSize indices=p.indices.empty() ? 0 : Upload(p.indices.data(),p.indices.size(),4);
    auto images=p.images;
    for (size_t i=0;i<4;i++) if (p.target_images[i]) {
        Image *image=MotionImageFor(p.target_images[i]);
        if (!image || !image->image) return;
        Barrier(*image,images[i].imageLayout); images[i].imageView=image->view;
    }
    BeginRendering(MotionTargetFor(p.rt),MotionTargetFor(p.ds));
    d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline);
    VkDescriptorBufferInfo uniform[2]={{ring_,vc,sizeof p.vc},{ring_,dc,sizeof p.dc}};
    VkDescriptorBufferInfo scene_positions{ring_,positions,p.scene.positions.empty()?16:p.scene.positions.size()*sizeof(ScenePosition)};
    VkWriteDescriptorSet writes[11]{};
    for (uint32_t i=0;i<11;i++) {
        auto& w=writes[i]; w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstBinding=i; w.descriptorCount=1;
        if(i==10) {w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&scene_positions;}
        else if (i<2) { w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.pBufferInfo=&uniform[i]; }
        else { w.descriptorType=i<6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
            w.pImageInfo=i<6 ? &images[i-2] : &p.samplers[i-6]; }
    }
    d_.vkCmdPushDescriptorSetKHR(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe_layout_,0,11,writes);
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
    // The boot/movie/menu graph changes viewport, video targets and 2D mode.
    // It must always use the authoritative original picture, even when a title
    // background happens to contain a few 3D draws.
    if (ngb_movie_active() || ngb_2d_screen_frame(int(threed_)) || threed_<30) return;
    if (!motion_capture_ || !motion_valid_ || !motion_3d_) return;
    motion_match_stats_=motion_history_.Resolve();
    motion_changed_=motion_match_stats_.changed;
#ifndef NGB_IOS_RELEASE
    motion_match_window_.draws+=motion_match_stats_.draws;
    motion_match_window_.models+=motion_match_stats_.models;
    motion_match_window_.shared+=motion_match_stats_.shared;
    motion_match_window_.unsupported+=motion_match_stats_.unsupported;
    motion_match_window_.fallback+=motion_match_stats_.fallback;
    motion_match_window_.rigid+=motion_match_stats_.rigid;
    motion_match_window_.cached+=motion_match_stats_.cached;
    motion_match_window_.fixed_rigid+=motion_match_stats_.fixed_rigid;
    motion_match_window_.fixed_skin+=motion_match_stats_.fixed_skin;
    motion_match_window_.input_cached+=motion_match_stats_.input_cached;
    motion_match_window_.vertices+=motion_match_stats_.vertices;
    motion_match_window_.evaluated_vertices+=motion_match_stats_.evaluated_vertices;
    motion_match_window_.extract_ms+=motion_match_stats_.extract_ms;
    motion_match_window_.interpolate_ms+=motion_match_stats_.interpolate_ms;
    motion_match_window_.project_ms+=motion_match_stats_.project_ms;
    motion_match_window_.cuts+=motion_match_stats_.cuts;
    motion_match_window_.vf.matched+=motion_match_stats_.vf.matched;
    motion_match_window_.vf.total+=motion_match_stats_.vf.total;
    motion_match_window_.vf.blended+=motion_match_stats_.vf.blended;
    motion_match_window_.cpu_ms+=motion_match_stats_.cpu_ms;
#endif
    if (!motion_changed_ || ngb_quick_wanted() || !motion_last_ns_ || now-motion_last_ns_>50000000LL) return;
    Target *target=EnsureTarget(front,false);
    if (!target || !motion_valid_) return;
    int slot=-1;
    for (int i=0;i<3;i++) {
        int candidate=(motion_display_next_+i)%3;
        if (!nv2a_present_external_image_busy(motion_display_[candidate].image)) { slot=candidate; break; }
    }
    if (slot<0) return;
    Image& midpoint_image=motion_display_[slot]; motion_display_next_=(slot+1)%3;
    // Original and midpoint commands share the ordered Vulkan queue, but write
    // separate targets. No CPU fence wait is needed between the two graphs.
    EndRendering();
    for (auto& command:motion_commands_) { command(); if (!motion_valid_) return; }
    EndRendering();
    Target *replayed=MotionTargetFor(target);
    if (midpoint_image.image && (midpoint_image.w!=target->img.w || midpoint_image.h!=target->img.h)) {
        Submit(); nv2a_vk_queue_lock(); d_.vkDeviceWaitIdle(dev_); nv2a_vk_queue_unlock();
        DestroyImage(midpoint_image);
    }
    if (!midpoint_image.image) CreateImage(midpoint_image,kColorFormat,target->img.w,target->img.h,1,1,false,false,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (midpoint_image.image) {
        if (gamma_on_ || bars>0 || bars_y>0) GammaCopy(replayed->img,midpoint_image,bars,bars_y);
        else MotionCopyImage(replayed->img,midpoint_image);
        Barrier(midpoint_image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    // EndFrame submits both graphs before the worker queues its blits on the
    // same queue. GPU completion, not a guest-thread sleep, protects the images.
    // Display cadence belongs to the presenter worker. Sleeping/acquiring twice
    // inside Xbox Swap made gameplay miss every other 60 Hz vblank.
    if (midpoint_image.image) motion_ready_=&midpoint_image;
    (void)endpoint;

}

void VulkanBackend::MotionFinish() {
    int64_t now=MotionClockNs();
    double hz=0;
#ifdef NGB_IOS_APP
    hz=ngb_ios_display_hz();
#endif
#ifndef NGB_IOS_RELEASE
    if (now-motion_log_ns_>=5000000000LL) {
        if (motion_log_ns_) std::fprintf(stderr,
            "[Smooth Motion] midpoint pairs queued %u/5s; eligible 3D draws %u, changed matches %u; packet %.1f MiB; snapshots %.1f MiB; valid %d, MSAA %u, custom glow %d, display %.1f Hz; guest clock 60 Hz\n",
            motion_pictures_,motion_3d_,motion_changed_,double(motion_cpu_bytes_)/1048576,double(motion_gpu_bytes_)/1048576,int(motion_valid_),msaa_,int(ContinuousGlowEnabled() || GlowSoftenMode()!=0),hz);
        uint64_t mids,ends,replaced; nv2a_present_pair_counts(&mids,&ends,&replaced);
        std::fprintf(stderr,"[Smooth Motion presenter] cumulative midpoint %llu, endpoint %llu, superseded pairs %llu\n",
            (unsigned long long)mids,(unsigned long long)ends,(unsigned long long)replaced);
#ifdef NGB_IOS_APP
        std::fprintf(stderr,"[Smooth Motion eligibility] requested %d; shared queue %d; scene draws %u; 2D %d; movie %d; quick menu %d; cooldown %u frames; EQUAL-depth draws %u\n",
            ngb_pc_setting(26),int(shared_),threed_,ngb_2d_screen_frame(int(threed_)),ngb_movie_active(),ngb_quick_wanted(),motion_budget_.cooldown,motion_equal_);
#endif
        std::fprintf(stderr,"[Smooth Motion VF3 scene/5s] core a8473a99091e unchanged; draws %u; models %u; shared passes %u; unsupported %u; centered models %u; rigid draws %u; cached meshes %u; cuts %u; VF3 matched %zu/%zu; vertex blends %zu; extraction+interpolation %.1f ms total\n",
            motion_match_window_.draws,motion_match_window_.models,motion_match_window_.shared,motion_match_window_.unsupported,
            motion_match_window_.fallback,motion_match_window_.rigid,motion_match_window_.cached,motion_match_window_.cuts,motion_match_window_.vf.matched,motion_match_window_.vf.total,
            motion_match_window_.vf.blended,motion_match_window_.cpu_ms);
        std::fprintf(stderr,"[Smooth Motion CPU/5s] fixed rigid draws %u; fixed skinned draws %u; vertices %u; interpreted vertices %u; unchanged input draws %u; extraction %.1f ms; VF3 interpolation %.1f ms; projection %.1f ms\n",
            motion_match_window_.fixed_rigid,motion_match_window_.fixed_skin,motion_match_window_.vertices,motion_match_window_.evaluated_vertices,motion_match_window_.input_cached,
            motion_match_window_.extract_ms,motion_match_window_.interpolate_ms,motion_match_window_.project_ms);
        motion_match_window_={};
        motion_log_ns_=now; motion_pictures_=0;
    }
#endif
    bool wanted=false;
#ifdef NGB_IOS_APP
    wanted=shared_ && ngb_pc_setting(26) && ngb_ios_display_hz()>=100;
#else
    const char *env=std::getenv("NGB_SMOOTH_MOTION"); wanted=shared_ && env && std::strcmp(env,"0");
#endif
    // Custom glow/MSAA paths need their own replay recipes. Keep their original
    // rendering intact rather than interpolate an incomplete graph.
    bool throttled=motion_budget_.cooldown && wanted;
    if (motion_budget_.Tick()) wanted=false;
    wanted=wanted && !ContinuousGlowEnabled() && GlowSoftenMode()==0 && msaa_==1 && threed_>=30 && !ngb_movie_active() &&
        !ngb_2d_screen_frame(int(threed_)) && !ngb_quick_wanted();
    bool reset=!motion_valid_ || !wanted || (motion_last_ns_ && now-motion_last_ns_>50000000LL);
#ifdef NGB_IOS_APP
    reset=reset || motion_epoch_!=ngb_ios_motion_epoch(); motion_epoch_=ngb_ios_motion_epoch();
#endif
    if (reset) { motion_history_.Reset(); nv2a_present_cancel_pairs(); }
    else motion_history_.Finish();
    // Keep isolated targets through a short overload pause and its warm-up
    // frame. Touch refreshes their contents before replay; cold allocations
    // must not create another stall immediately after recovery.
    bool keep_targets=throttled || (motion_recovering_ && wanted);
    motion_recovering_=throttled;
    bool prune=reset && !keep_targets;
    for (auto& [key,saved]:motion_targets_) if (frame_-saved.seen>2) prune=true;
    if (prune && !keep_targets && !motion_targets_.empty()) {
        Submit();
        for (auto it=motion_targets_.begin();it!=motion_targets_.end();) {
            if (reset || frame_-it->second.seen>2) {
                auto& p=it->second;
                motion_gpu_bytes_-=p.replay.img.image ? uint64_t(p.replay.img.w)*p.replay.img.h*(p.replay.img.depth ? 8 : 4) : 0;
                if (p.replay.fb.image) motion_gpu_bytes_-=uint64_t(p.replay.fb.w)*p.replay.fb.h*4;
                DestroyImage(p.replay.img); DestroyImage(p.replay.fb); it=motion_targets_.erase(it);
            } else ++it;
        }
        if (motion_targets_.empty()) motion_gpu_bytes_=0;
    }
    motion_commands_.clear(); motion_cpu_bytes_=0; motion_3d_=motion_changed_=motion_equal_=0; motion_match_stats_={};
    motion_capture_=motion_valid_=wanted; motion_last_ns_=now;
}
#endif
