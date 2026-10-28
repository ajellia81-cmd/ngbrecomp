#!/usr/bin/env python3
"""Compile the exact driver-patch conditions against synthetic Vulkan state."""
import argparse,ast,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--vulkan-include',type=Path,required=True);parser.add_argument('--compiler',default=os.environ.get('CXX','c++'));args=parser.parse_args()
tree=ast.parse((root/'porting/macos-on-ios/universal/patch_moltenvk.py').read_text())
parts={t.id:ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) for t in n.targets if isinstance(t,ast.Name) and t.id in ('patch','barrier_patch')}
head=r'''
#include <vulkan/vulkan.h>
#include <array>
#include <cassert>
#include <vector>
struct Capabilities {bool isAppleGPU=true;};
struct Physical {Capabilities caps; const Capabilities& getMTLDeviceCapabilities(){return caps;}};
namespace spv {enum ExecutionModel {ExecutionModelFragment,ExecutionModelVertex};}
struct Resource {struct {spv::ExecutionModel stage=spv::ExecutionModelFragment;uint32_t desc_set=0,binding=28,msl_texture=29;} resourceBinding;};
struct Descriptor {VkDescriptorType descriptorType=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;};
struct Layout {Descriptor descriptor;size_t getDescriptorSetCount(){return 1;}Layout* getDescriptorSetLayout(uint32_t){return this;}Descriptor* getBinding(uint32_t){return &descriptor;}};
struct Configuration {struct {struct {bool use_framebuffer_fetch_subpasses=false;} mslOptions;} options;std::vector<Resource> resourceBindings{Resource{}};};
struct Harness {
 Layout layout;Layout* _layout=&layout;uint32_t remapped=29;Physical physical;bool _inputAttachmentIsDSAttachment=false,_usesNativeColorFetch=false;
 Physical* getPhysicalDevice(){return &physical;}
 bool convert(const VkGraphicsPipelineCreateInfo* pCreateInfo){Configuration shaderConfig;
'''
middle=r'''
 remapped=shaderConfig.resourceBindings[0].resourceBinding.msl_texture;
 return shaderConfig.options.mslOptions.use_framebuffer_fetch_subpasses;
 }
};
struct MVKPipelineBarrier {
 enum Type {Memory,Image};Type type=Memory;
 VkPipelineStageFlags2 srcStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,dstStageMask=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
 VkAccessFlags2 srcAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,dstAccessMask=VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
 VkImageLayout newLayout=VK_IMAGE_LAYOUT_UNDEFINED;
};
struct Pipeline {bool native;bool usesNativeColorFetch(){return native;}};
struct Encoder {Pipeline* pipeline;Pipeline* getGraphicsPipeline(){return pipeline;}};
bool restart(MVKPipelineBarrier barrier,bool native,bool region=true,bool bound=true) {
 Pipeline pipeline{native};Encoder encoder{bound?&pipeline:nullptr};auto* cmdEncoder=&encoder;
 std::array<MVKPipelineBarrier,1> barriers{barrier};
 for(auto& b:barriers) {
  if(b.type==MVKPipelineBarrier::Image && b.newLayout==VK_IMAGE_LAYOUT_GENERAL)return true;
  if(region && (b.dstAccessMask&VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT)) {
'''
tail=r'''
   return true;
  }
 }
 return false;
}
int main(){Harness h;VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
 assert(!h.convert(&pipeline));
 uint32_t index=0;VkRenderingInputAttachmentIndexInfoKHR local{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};local.colorAttachmentCount=1;local.pColorAttachmentInputIndices=&index;
 VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.pNext=&local;pipeline.pNext=&rendering;
 assert(h.convert(&pipeline));assert(h.remapped==0);
 h.layout.descriptor.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;assert(h.convert(&pipeline));assert(h.remapped==29);h.layout.descriptor.descriptorType=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
 h._inputAttachmentIsDSAttachment=true;assert(!h.convert(&pipeline));h._inputAttachmentIsDSAttachment=false;
 h.physical.caps.isAppleGPU=false;assert(!h.convert(&pipeline));h.physical.caps.isAppleGPU=true;
 index=VK_ATTACHMENT_UNUSED;assert(!h.convert(&pipeline));index=1;assert(!h.convert(&pipeline));index=0;
 local.colorAttachmentCount=2;assert(!h.convert(&pipeline));local.colorAttachmentCount=0;assert(!h.convert(&pipeline));local.colorAttachmentCount=1;
 local.pColorAttachmentInputIndices=nullptr;assert(!h.convert(&pipeline));
 MVKPipelineBarrier b;assert(!restart(b,true));assert(restart(b,false));assert(restart(b,true,true,false));assert(!restart(b,true,false));
 b.srcStageMask|=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;assert(restart(b,true));b={};
 b.srcAccessMask|=VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;assert(restart(b,true));b={};
 b.dstAccessMask|=VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;assert(restart(b,true));b={};
 b.type=MVKPipelineBarrier::Image;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;assert(restart(b,true));
}
'''
with tempfile.TemporaryDirectory(prefix='ngb-native-fetch-scope-') as temp:
    work=Path(temp);(work/'scope.cpp').write_text(head+parts['patch']+middle+parts['barrier_patch']+tail)
    subprocess.run([args.compiler,'-std=c++17','-I'+str(args.vulkan_include),str(work/'scope.cpp'),'-o',str(work/'scope')],check=True)
    subprocess.run([str(work/'scope')],check=True)
print('PASS: native fetch and no tile restart only for explicit Apple color0/input0 and its exact dependency; ordinary/depth/other barriers preserved')
