# VF3 scene interpolation reference

Source: https://github.com/ajellia81-cmd/vf3-recomp
Commit: a8473a99091e6bb207c85d6d247eece8c4866b39
License: MIT; see LICENSE.

The reference r3d_scene.cpp and r3d_scene.h are unchanged. interpolate.cpp
contains the original Mat4 definitions and the entire original interpolation
section, without algorithm changes. Only the includes omit the Model3 hardware
reader. The iOS NGB renderer calls this exact implementation through gpu/scene_motion.cpp.

The adapter snapshots NGB's camera and evaluates the original vertex program's
position calculation, including relative bone addressing. It converts those
positions into model-space meshes with model-to-view matrices and viewports,
then projects the VF3 result into a replay-only vertex buffer. Rigid position
programs are reduced algebraically to their complete matrix, with finite-domain
checks for perspective division; nonlinear/skinned programs use the original
position evaluator. Unchanged rigid meshes retain their shared scene geometry. Material/lighting
varyings still come from the original NGB vertex program. Offscreen effects
keep the original recipes. The former constant-bank matcher is removed.

Run tests/check_vf3_reference.py to check the pinned source SHA256 and byte-for-byte
identity of the extracted interpolation section. The hardware reader is not
compiled: it requires Model3, whereas NGB submits its scene through Direct3D.
