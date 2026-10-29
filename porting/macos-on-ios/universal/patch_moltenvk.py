#!/usr/bin/env python3
"""Native color0 fetch, retaining its Vulkan dependency without a tile-pass break."""
from pathlib import Path
import argparse,hashlib,json,subprocess
BASE='db66022459ffb663aa2b50f6b018bc2e124f5edf'
parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);args=parser.parse_args()
assert subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip()==BASE
paths=['MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm','MoltenVK/MoltenVK/GPUObjects/MVKPipeline.h','MoltenVK/MoltenVK/Commands/MVKCmdPipeline.mm']
sources={}
for name in paths:
    original=subprocess.check_output(['git','-C',str(args.source),'show',BASE+':'+name],text=True)
    assert (args.source/name).read_text()==original,'Patch requires untouched pinned source: '+name
    sources[name]=original

def insert(name,anchor,replacement):
    assert sources[name].count(anchor)==1,(name,anchor)
    sources[name]=sources[name].replace(anchor,replacement)

patch='''
        // NGB: a single color0/input0 local-read mapping uses native fetch.
        // Texture reads cannot observe earlier overlapping primitives in a draw.
        bool nativeColorInput = false;
        for (auto* next = reinterpret_cast<const VkBaseInStructure*>(pCreateInfo->pNext);
             next; next = next->pNext) {
            if (next->sType == VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR) {
                auto* local = reinterpret_cast<const VkRenderingInputAttachmentIndexInfoKHR*>(next);
                nativeColorInput = local->colorAttachmentCount == 1 &&
                    local->pColorAttachmentInputIndices &&
                    local->pColorAttachmentInputIndices[0] == 0;
            }
        }
        if (nativeColorInput && !_inputAttachmentIsDSAttachment &&
            getPhysicalDevice()->getMTLDeviceCapabilities().isAppleGPU) {
            shaderConfig.options.mslOptions.use_framebuffer_fetch_subpasses = true;
            // SPIRV-Cross honors explicit resource remaps before the input
            // attachment index. Native fetch needs color0, not a texture slot.
            for (auto& resource : shaderConfig.resourceBindings) {
                auto& binding = resource.resourceBinding;
                if (binding.stage == spv::ExecutionModelFragment &&
                    binding.desc_set < _layout->getDescriptorSetCount() &&
                    _layout->getDescriptorSetLayout(binding.desc_set)->getBinding(binding.binding)->descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                    binding.msl_texture = 0;
                }
            }
            _usesNativeColorFetch = true;
        }
'''
anchor='\t\tshaderConfig.options.mslOptions.input_attachment_is_ds_attachment = _inputAttachmentIsDSAttachment;\n'
insert(paths[0],anchor,anchor+patch)
# Native color inputs are render-pass arguments, not sampled textures. The
# remapped color index must never become a texture bind operation (push sets
# bind resources directly, so color0 would otherwise overwrite texture slot0).
unbind_patch='''
        if (_usesNativeColorFetch) {
            for (auto& resource : shaderConfig.resourceBindings) {
                const auto& binding = resource.resourceBinding;
                if (binding.stage == spv::ExecutionModelFragment &&
                    binding.desc_set < _layout->getDescriptorSetCount() &&
                    _layout->getDescriptorSetLayout(binding.desc_set)->getBinding(binding.binding)->descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                    resource.outIsUsedByShader = false;
                }
            }
        }
'''
anchor='\t\tplDesc.fragmentFunction = mtlFunc;\n\t\tif ( !mtlFunc ) { return false; }\n'
insert(paths[0],anchor,anchor+unbind_patch)
anchor='\tbool isRasterizationDisabled() const { return !_isRasterizing; }\n'
insert(paths[1],anchor,anchor+'\tbool usesNativeColorFetch() const { return _usesNativeColorFetch; }\n')
anchor='\tbool _inputAttachmentIsDSAttachment = false;\n'
insert(paths[1],anchor,anchor+'\tbool _usesNativeColorFetch = false;\n')

barrier_patch='''
                // Native color fetch implements this exact pixel dependency
                // without storing/reloading the tile after every generic draw.
                if (b.type == MVKPipelineBarrier::Memory &&
                    b.srcStageMask == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT &&
                    b.dstStageMask == VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT &&
                    b.srcAccessMask == VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT &&
                    b.dstAccessMask == VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT) {
                    auto* pipeline = cmdEncoder->getGraphicsPipeline();
                    if (pipeline && pipeline->usesNativeColorFetch()) continue;
                }
'''
anchor='''\t\t\tif (mvkIsAnyFlagEnabled(_dependencyFlags, VK_DEPENDENCY_BY_REGION_BIT) &&
\t\t\t\tmvkIsAnyFlagEnabled(b.dstAccessMask, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)) {

\t\t\t\tneedsRenderpassRestart = true;'''
replacement=anchor.replace('\n\n\t\t\t\tneedsRenderpassRestart', '\n'+barrier_patch+'\n\t\t\t\tneedsRenderpassRestart')
insert(paths[2],anchor,replacement)
for name,text in sources.items():(args.source/name).write_text(text)
print('MoltenVK 1.4.2 scoped native-fetch patch:',json.dumps({n:hashlib.sha256(t.encode()).hexdigest() for n,t in sources.items()},sort_keys=True))
