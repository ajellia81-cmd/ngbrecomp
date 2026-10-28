# VF3 production scene interpolation

Source: https://github.com/ajellia81-cmd/vf3-recomp
Branch: ios-port
Commit: 842151aaf97a4bf5e59709004a385ae5735c5a05
License: MIT; see LICENSE.

r3d_scene.cpp and r3d_scene.h are byte-for-byte upstream source. interpolate.cpp
extracts the original Mat4 definitions and complete interpolation tail without
algorithm changes. The Model3 memory reader is not compiled for Xbox input.
Run tests/check_vf3_reference.py to verify source hashes and extraction identity.
The latest upstream flipbook particle/snap exclusions are included, with the
actual upstream production-pass regression in tests/vf3_effect_flipbook.cpp.
That fixture changes only its include path to the extracted translation unit.

The NGB Direct3D bridge supplies owned geometry, real camera/model transforms,
viewports and immutable mesh identities to video::interpolate at t=0.5. It then
projects the returned scene for isolated Vulkan replay. This is an adapter to
the same algorithm, not Model3 hardware emulation. The original vertex programs
still supply endpoint lighting/UV/materials. VF3 flipbook texture crossfading is
not implemented by NGB's material renderer; this is not an all-effects guarantee.
