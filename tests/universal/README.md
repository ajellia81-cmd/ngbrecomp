# Generic shader prototype tests

These tests render synthetic geometry offscreen and compare the original
specialized generator with the generic generator. They require no disc, game
files, saves, windows, sound or recorded shader recipes.

Build with Vulkan headers/loader and glslang 16 (the production compiler API):

```sh
cmake -S tests/universal -B /tmp/ngb-universal-build -G Ninja
cmake --build /tmp/ngb-universal-build
mkdir -p /tmp/ngb-universal-cache
XDG_CACHE_HOME=/tmp/ngb-universal-cache MESA_SHADER_CACHE_DISABLE=true \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
ctest --test-dir /tmp/ngb-universal-build --output-on-failure
```

Select an offscreen Vulkan ICD explicitly with `VK_ICD_FILENAMES` where
necessary. Do not use the game's real cache directories. The device must expose
Vulkan 1.3 and VK_KHR_dynamic_rendering_local_read. On non-Metal devices the
ordered overlap test also needs rasterization-order color attachment access;
this is not a substitute for checking the packaged MoltenVK on iPhone.

The 287 comparisons cover pretransformed geometry, fixed texture-stage
operations, generated register-combiner states, an NV2A MOV program, framebuffer
fetch, same-draw overlapping additive primitives, randomized blend equations,
factors and channel write masks, all fixed skin modes, three light types, and
bidirectional specialized/generic depth EQUAL passes. Differences greater than one UNORM8 step fail.
This does not yet establish full instruction/format, MSAA, shadow, or Smooth
Motion replay equivalence. The test executable does not enable game rendering.
No first-use frame-time claim is justified by these image comparisons.
