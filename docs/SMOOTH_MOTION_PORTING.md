# Smooth Motion handoff for a new project

Audience: the agent implementing another port. Read this file before copying
code or changing the interpolation algorithm. This is a source handoff, not a
claim that an Xbox renderer can be dropped into a different game unchanged.

## Start here

1. Run `python3 tools/export_smooth_motion.py /path/out/smooth-motion-kit.tar.gz`
   from a committed checkout. The archive has an explicit public-source
   allowlist and a SHA-256 manifest. It does not contain game data, generated
   game C, user saves, device logs, captures, credentials or build outputs.
2. Start with `third_party/vf3_scene/interpolate.cpp`, its header and license.
   Compile `tests/vf3_scene_test.cpp` against that source before integrating a
   renderer. Run `python3 tests/check_vf3_reference.py` too.
3. Treat the NGB adapter, Vulkan replay and iOS presenter as implementation
   references. They depend on NGB's renderer and cannot be linked as a generic
   drop-in library. Supply the new game's real geometry and transforms through
   an adapter matching the contract below.
4. First validate unchanged endpoint rendering, then scene conversion, then
   midpoint correctness, and finally high-refresh pacing and sustained load.
   Do not debug all four layers by changing interpolation tolerances together.

Standalone common-core check (no game files, GPU, window or audio):

```sh
python3 tests/check_vf3_reference.py
c++ -std=c++20 -O2 tests/vf3_scene_test.cpp \
    third_party/vf3_scene/interpolate.cpp -o /tmp/vf3-scene-test
/tmp/vf3-scene-test
```

## What is actually shared with VF3

Reference: `ajellia81-cmd/vf3-recomp`, commit
`a8473a99091e6bb207c85d6d247eece8c4866b39`, MIT license.

`r3d_scene.cpp` and `r3d_scene.h` are unchanged. `interpolate.cpp` contains the
original Mat4 definitions and complete interpolation tail, with the Model3
hardware reader omitted. The identity checker verifies both the original
source SHA-256 and the exact extracted implementation. Keep the license with
redistributed source and applications.

This shares viewport/model pairing, repeated-resource handling, camera-motion
consensus, scene-cut decisions, matrix/compatible-vertex interpolation and
cached plans. It does not share Model3's hardware reader or the complete VF3
renderer/material animation. Different hardware needs a different adapter;
that does not require replacing the common interpolation algorithm.

## Scene adapter contract

- Capture real per-draw inputs, indices, declarations, viewports and matrices
  while their contents are valid. The scene must own all CPU data it retains.
- Supply model-space geometry and actual model-to-view matrices. A draw's
  constant bank is not a reliable model identity. Draw order and stream
  addresses are not reliable instance identity either.
- A resource can have multiple visible instances. Keep their real spatial
  transforms so swapped draw order cannot swap two lamps or walls.
- Use one coordinate convention at the adapter boundary. VF3 uses column-major
  matrices and a -Z view. NGB converts Xbox +Z view coordinates there and
  reverses that conversion when projecting output. Copying this sign change
  into a game already using -Z would be incorrect.
- Every Scene ID must be unique across the process, including after adapter
  recreation. The common implementation caches plans by Scene ID. NGB uses a
  process-wide atomic serial to avoid stale-plan collisions.
- Share immutable mesh objects across frames. Fresh mesh pointers for static
  geometry can force compatible-vertex blending on thousands of vertices.
- Canonicalize viewport order independently of draw order. Keep main-camera
  geometry separate from offscreen/shadow-camera recipes.
- If skinning produces posed view/world-space vertices, recover a consistent
  local space and real spatial origin. Do not represent all such instances at
  an identity origin. NGB uses the posed bounding-box center where necessary.

NGB's adapter is `src/d3d/gpu/scene_motion.cpp` and `.h`. Its original-position
reference function and fixtures are in `ios/motion_test.cpp`.

## Vertex conversion and immutable inputs

Prefer the renderer's exact fixed-function world/view/projection matrices for
rigid terrain. For programmable input, NGB slices the original position program
backwards from oPos. It proves a rigid affine transform algebraically; nonlinear
or unproven programs retain their original evaluator. Perspective reciprocal
cancellation requires finite-domain checks. A few sampled vertices are not a
proof of the full transform.

Fixed skinning prepares matrices/state once per draw. Explicit bone weights
need not sum to one; keep the original point-W rule separate from that sum.
Decode raw formats and homogeneous positions exactly as the original vertex
shader does. The NGB fixture checks SHORT3 and all six fixed blend modes.

A cache hit must compare owned input bytes as well as declarations, decoding,
topology and indices. UV values matter to the common mesh matching too. A
buffer at the same address can have changed contents; relocated identical
contents can still be reusable. Transforms remain per-draw. Retain bounded
storage and fall back safely when a draw exceeds a cap.

NGB performance lesson: optimizing only programmable rigid shaders missed most
terrain, which used fixed function. Measure the dominant input path instead of
assuming the shader path is dominant. The 2.63 fixed-input cache skips repeated
decoding, triangle expansion and mesh hashing, rather than merely caching the
last step. Do not use the CPU fixture's timing as an iPhone FPS guarantee.

## Replay and presentation contract

The guest simulation, vblank, audio and visibility queries stay at 60 Hz.
Only presentation adds an intermediate scene. In NGB the unchanged common
function interpolates the two scenes at 0.5, and the adapter projects its result
into a replay-only storage buffer. The authoritative endpoint draw never uses
that override. Original constants and geometry are not averaged in place.

Replay must own its command inputs and pin immutable texture versions. Render
intermediate commands into isolated targets, without changing guest-visible
pixels, queries or side effects. Preserve render ordering, depth/stencil,
material and feedback passes. Shared geometry/pose across material/depth passes
must yield the same position result, including EQUAL-depth draws.

The replay wrapper changes position output only. NGB's original endpoint
program still supplies lighting, UV and other varyings. This is a documented
limit, not a promise of complete material-animation interpolation.

NGB uses the same ordered Vulkan queue for original/replay work and presentation
copies. The presenter owns display cadence; do not sleep for two drawables on
the guest thread or accidentally double the guest clock. Use a latest-pair
mailbox instead of an unbounded FIFO. Source-image leases last until GPU copy
fences retire, even after a newer pair supersedes an older pair.

A whole replay draw, including scene positions, uniforms and all vertices and
indices, must fit in one upload-ring slot. Testing only the size of the whole
two-slot allocation can reset uniforms halfway through a draw. See
`src/d3d/gpu/upload_ring.h` and the actual replay reservation.

At cuts, long gaps, lifecycle changes, resize or unsupported graphs, cancel
queued pairs and reset history. Never interpolate across a recovery pause.
Target/texture lifetime must remain valid even during skipped frames; refresh
snapshots before reuse and retire resized targets safely. Do not copy a stale
GPU pointer into a new project's lifetime scheme.

Current NGB caps: 96 MiB command packets, 64 MiB frame scene geometry, 32 MiB
input/expanded-geometry cache, 32 MiB rigid-mesh cache and 256 MiB replay targets.
These are NGB limits, not universal recommended settings.

## Performance and overload

Measure separately: guest Swap rate, actual midpoint/endpoint submissions,
CPU extraction, common interpolation, output projection, GPU render/copy cost,
drawable acquisition and source-lease waits. A 120 Hz callback is not evidence
of 120 unique rendered frames. Distinguish output FPS from the 60 Hz game rate.

`src/d3d/gpu/motion_budget.h` is NGB's small tested overload policy: four
consecutive replay-associated intervals above 25 ms and below 100 ms trigger a
12-guest-frame recovery. Normal intervals, original-only frames and suspension
break the streak. The old two-spike/120-frame pause caused noticeable dropouts.
Short recovery retains allocated targets through warm-up but resets scene
history. Reassess these numbers against the new renderer's measured budget.

Do not remove protection to conceal missing midpoints if the game itself falls
below 60 Hz. Reduce measured CPU/GPU overhead first. Doubling scene rendering
still adds GPU work; sustained 120 FPS is not guaranteed on every scene/device.

## Shipping diagnostics policy

The shipping iOS Release app disables only diagnostics introduced by this port:
automatic build-info files, startup sampling, adapter performance timers and
additional iOS/presenter reports. The original logger, profiler, error output
and F7/env-triggered recorder are preserved on iOS and desktop builds. Do not
silence standard streams globally or disable a baseline feature just because
it is useful for debugging. Use native fixtures or an explicit diagnostic
configuration when measuring port-only telemetry. Functional caches, Korean
data, framework contents and license notices remain intact.

## Regression checklist for the next agent

- Compare imported/projected positions to the original vertex calculation.
- Exercise reordered identical resources, camera translation and rotation,
  animated/skinned geometry, depth/material passes and real scene cuts.
- Test changed bytes at an unchanged stream address, identical relocated
  streams, raw declarations, index changes and independently changed matrices.
- Test history reset and adapter recreation, including Scene ID uniqueness.
- Compile replay shader wrappers to SPIR-V and compile the real device renderer.
- Stress mailbox supersession, source-image ownership/fences, upload slot
  boundaries, resize, background/resume and short overload recovery.
- Measure sustained loaded scenes with Smooth Motion Off and On. Preserve
  game speed before claiming high-refresh success. Retest graphics on a device;
  CPU fixtures and a successful IPA build do not establish visual correctness.

The kit contains the common-core tests and NGB's adapter/presenter references.
NGB-only tests require a full NGB checkout and its build dependencies. Begin the
new project with the standalone core check above, then implement equivalent
fixtures for its own adapter. Preserve accepted unrelated UI/font work.
