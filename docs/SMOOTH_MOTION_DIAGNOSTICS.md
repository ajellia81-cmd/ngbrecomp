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
material candidates; not every candidate is necessarily water. No game code,
texture pixels, disc inputs, recorded shader recipes or device paths are logged.

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

Only render targets written by the current replay use a replay image. A target
sampled without being updated must use its unchanged original image, including
feedback targets without a feedback copy in the current frame. Otherwise an old
midpoint image can leak into water/reflection draws, or an absent clone can drop
a draw. Actual water appearance and distant-scene frame rate still need device
verification; synthetic scene tests do not establish visual correctness.
