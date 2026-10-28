# Smooth Motion on the macOS-on-iOS baseline

Integration baseline: confirmed 3.0.14, commit 8803ebeb3961ea5c0e2d1e28f5bdeef8530df317.
VF3 production core: ios-port 842151aaf97a4bf5e59709004a385ae5735c5a05, MIT.

## Reuse boundary

Use the unchanged VF3 scene pairing, repeated-instance matching, camera-motion
consensus, scene-cut handling, matrix/vertex interpolation and cached plans.
The extraction checker pins the actual latest upstream source, including the
particle/snap exclusion fix in its flipbook pass. The Model3 hardware reader
cannot read Xbox Direct3D inputs; NGB supplies the scene through scene_motion.
The NGB-specific vertex evaluator/geometry bridge and isolated Vulkan replay
provide those inputs and consume the common result. Position output only is
overridden. Original endpoint lighting, materials and UV evaluation remain.
This is not a claim that VF3's full renderer can run unchanged on Xbox draws.

## Changes deliberately excluded

No old NGB loading acceleration, shader-ready/cache-producer/demo gates,
additional motion prewarm, altered shader recipe recording, synchronous compile
policy or adaptive overload cooldown. The long-loading Mac prewarm/cache methods
are unchanged. Guest scheduler/kernel, audio, Korean startup handshake, app name,
icon, save/data paths and 3.0.14 asynchronous presentation policy are retained.

## Presentation and lifetime

Simulation/vblank/audio remain 60 Hz. Eligible high-refresh gameplay replays an
intermediate 3D scene then shows the authoritative endpoint. The bounded display
mailbox replaces pending pairs without losing active GPU-copy leases. Originals
and replay use the same ordered Vulkan queue. Replay targets are isolated from
guest-visible targets and queries. All CPU packet data is owned and immutable
textures are pinned. Each replay draw fits entirely in one upload-ring slot.
Scene cuts, movies, menus, unsupported MSAA/glow graphs, resize and lifecycle
changes cancel history/pairs. Default is Off; saved user choice is respected.

## Verification

check_baseline.py guards original Mac prewarm/cache and CPU timing. The endpoint
shader test compares actual generated HLSL with Off and On against the original
Mac shader generator. VF3 fixtures exercise camera/repeated-instance/cached
pairing and the production effect exclusion pass. motion_test.cpp checks fixed
and programmable positions, bone addressing, shared-depth passes and raw inputs.
Presenter fixtures cover coherent publication, lease retirement, stale pair
cancellation and actual submitted-picture FPS accounting. CI compiles emitted
HLSL to SPIR-V and builds macOS ARM64 then converts/packages the real iOS app.
Device visual correctness, actual 120 FPS and sustained performance still require
owner testing; CADisplayLink frequency alone does not establish unique pictures.
