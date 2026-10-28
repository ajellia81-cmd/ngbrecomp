#!/usr/bin/env python3
"""Enable native color-attachment fetch only for explicit local-read pipelines."""
from pathlib import Path
import argparse,hashlib,subprocess
BASE='db66022459ffb663aa2b50f6b018bc2e124f5edf'
parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);args=parser.parse_args()
assert subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip()==BASE
path=args.source/'MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm'
source=path.read_text()
original=subprocess.check_output(['git','-C',str(args.source),'show',BASE+':MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm'],text=True)
assert source==original,'Patch requires the untouched pinned source'
anchor='\t\tshaderConfig.options.mslOptions.input_attachment_is_ds_attachment = _inputAttachmentIsDSAttachment;\n'
assert source.count(anchor)==1
patch='''
        // NGB: color input attachments in an explicit local-read pipeline must
        // use native framebuffer fetch. Sampling the attached texture does not
        // observe preceding overlapping primitives within the same draw.
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
        }
'''
path.write_text(source.replace(anchor,anchor+patch))
print('MoltenVK 1.4.2 native color-fetch patch:',hashlib.sha256(path.read_bytes()).hexdigest())
