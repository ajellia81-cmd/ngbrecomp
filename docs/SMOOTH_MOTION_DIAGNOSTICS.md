# Smooth Motion diagnostic build

Baseline: compact distribution 3.1.30, commit d01e95698ff2bd0bf65d8935a0bf75f102be0a8f.
Enable `NGB_IOS_MOTION_DIAGNOSTICS` for the optimized diagnostic build. Keep
`NGB_IOS_PORT_RELEASE` enabled: this avoids restoring broad startup/kernel logs.
The distribution build leaves diagnostics disabled.

The iOS bridge supplies a sandbox Documents path for
`smooth-motion-diagnostics.log`. Each launch replaces the previous log.
The render thread queues summaries once per second to a native GCD serial disk writer, started only after the first game frame.
The four-slot queue drops diagnostics when full instead of blocking gameplay;
`log_dropped` reports this. There are no GPU
queries, completion waits, screenshots, frame holds or shader-policy changes.
Bump replay tags live in a separate bounded side table, preserving the original
replay packet size and memory-budget decisions. Overflow is reported as
`packet_metadata_dropped`. The file stops growing at approximately 8 MiB. At most eight bump signatures
are described per interval; overflow is counted.

Record the stage-2 opening with Smooth Motion enabled, then the flickering
water surface. Compare the same area with Smooth Motion disabled in the same
session. Export the log before restarting the app; a short screen recording
helps locate the visible symptom. Do not publish user logs or recordings.

`[frame]` records elapsed/wall time, guest cadence, display cadence, resolution,
MSAA, long frame counts, option/eligibility state, reset counts, CPU phase time,
packet/snapshot high-water marks, and presenter image counters. CPU midpoint
time includes replay time, so those columns must not be summed. This is not a
GPU profiler: unaccounted frame time cannot alone prove a GPU bottleneck.

`[bump]` records hashed pixel-state keys, texture modes, signed/target/depth
texture masks, blend/depth state and endpoint/replay counts. These are bump
material candidates; not every candidate is necessarily water. Texture pixels,
guest CPU code, disc inputs and device paths are not logged. The targeted
private vertex-program diagnostic is described below.

## Early-exit investigation

The diagnostic build records C-only executable-initializer and conversion-entry
markers in `ngb-startup-diagnostics.log`. The original game host main source
remains unchanged; reaching conversion startup confirms main called it. It first uses HOME/Documents and
mirrors the markers into the game Documents path when Foundation is available.
Platform preparation stages also appear briefly in an on-screen label. This
cannot record a loader rejection that occurs before any executable initializer.

The v2 writer uses native GCD already used by the port, eliminating the new
C++ writer thread and timed condition-variable wait introduced in3.1.34. This
is a compatibility isolation change, not proof of the user's exit cause.
# Terrain capture and sampled targets

The diagnostic build reports a `[scene-input]` row beside each one-second frame
summary. `key_MiB` counts bytes copied into fixed-geometry cache keys; `hits`
counts unchanged fixed inputs. `target_readonly` and `target_replay` count
sampled render targets using their unchanged authoritative image or the current
midpoint image, respectively. These are CPU-side counters, not GPU timings.

Fixed terrain keys retain the indexed draw's vertex interval instead of an
entire shared backing buffer, and retain interleaved position/UV bytes once.
Index data, declarations, decoding and consumed bytes still determine cache
identity; changing consumed geometry invalidates the cache even at the same
address. This does not change VF3 matching, interpolation or the guest clock.

The defensive original-image fallback did not trigger in the reported water
scene: `BindTexture` touches sampled render targets too. A zero `target_readonly`
count therefore does not establish a water fix. Actual water appearance and
distant-scene frame rate need device verification; synthetic scene tests do not
establish visual correctness.

## Bounded caches and remaining water investigation

Fixed terrain no longer clears its entire cache at 32 MiB. An LRU retains recent
owned inputs/geometry within a 128 MiB hard payload budget. `cache_MiB`, `evictions`
and `fixed` distinguish cache pressure from a different draw mix. Scenes and
other renderer caches can retain shared geometry independently; this is not a
bound on the entire renderer's memory.

Vertex-program matrix proofs cache exact position-dependent constant bytes plus
viewport normalization c58/c59. Relative addressing conservatively captures all
constants. Hash collisions require an exact byte match, and changed inputs rerun
the original algebraic proof. The VF3 algorithm and proof result are unchanged.
`[scene-work]` reports proof time/count/hits, decoded/interpreted vertices and
fixed skin draws so expensive work can be identified without GPU waits.

For bump candidates sampling a render target, `[water-state]` records vertex
shader identity, programmable/fixed mode, viewport, stage-2 coordinate generation
and transformation flags, projective sampling and feedback use. `[water-program]`
stores at most 1024 vertex-program words for each of eight unique variants, once
per process. These runtime shader words are proprietary user data: keep the log
private and never commit it or publish it as a CI artifact. No pixels are read
back, and no synchronous GPU waits are added. This is diagnostic evidence, not
a claim that the remaining water flicker has been fixed.
