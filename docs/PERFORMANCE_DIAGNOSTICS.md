# Frame-rate and stutter diagnostic build

Build the same Release engine with `NGB_IOS_PORT_RELEASE=ON`,
`NGB_IOS_MOTION_DIAGNOSTICS=ON`, and `NGB_IOS_PERFORMANCE_DIAGNOSTICS=ON`.
Set both packaging variables `NGB_MOTION_DIAGNOSTIC_BUILD=1` and
`NGB_PERFORMANCE_DIAGNOSTIC_BUILD=1`. The shader compiler, prewarm/cache,
game clock, rendering decisions, VF3 interpolation and water fix stay unchanged.

Output: Documents/smooth-motion-diagnostics.log, format v3. Each launch replaces
this file. Copy it after reproducing the problem and before launching again.
No image captures, shader words, per-draw timers, automatic thread sampler,
startup overlay or shim polling/compiler messages are enabled in this mode.

The render thread accumulates coarse CPU wall times in fixed-size counters.
At most one summary batch per second is copied to a utility-priority native GCD
writer. There are at most four pending batches and an 8 MiB file limit; a full
queue drops a batch rather than waiting. A summary retains at most eight frame
intervals over 25 ms. Threshold counters and the interval peak include all frames.

`game_hz` counts completed Presents, after existing queue submission/fence waits.
`display_hz` is measured display cadence, not a count of presented pictures.
Midpoint/endpoint counters expose the actual presenter's cumulative submissions.
`requested` is the user's setting; `wanted` also reflects scene eligibility.

`outside_present_ms` includes game work, draw recording and scheduling between
Presents; it cannot isolate guest CPU work. `present_ms` measures inside Present,
including EndFrame. `resolve_ms` measures scene matching/projection together.
`gpu_fence_wait_ms` measures CPU time spent in existing fence waits, not GPU
execution time. `cleanup_ms` covers retired resources and texture eviction.
Sync compile counters are captured before the original per-frame reset.
Compiler-worker rows measure background job elapsed time and may cross a report
boundary. Worker counters are atomic; render-thread data is never accessed there.

**Phases overlap.** Resolve is inside midpoint; fence waits/queue submission are
inside EndFrame or synchronous Submit. Do not sum every phase into a frame time.
The slow-frame rows correlate these values for a specific completed frame.
`previous_logging_ms` measures report formatting/enqueue CPU work in the preceding
frame interval. It does not measure the background writer's disk latency or
prove diagnostic overhead is zero. Compare with the clean release when needed.

Synthetic tests cover an injected 120 ms interval with a 30 ms fence wait,
correlated inside/outside Present times, bounded slow rows/batches, compiler
counter handoff, no per-draw sampling, and absence of startup file/UI markers.
The package audit rejects unrelated water/automatic-sampler/polling diagnostics.
