# Frame diagnostic build

Base engine: upstream v0.9.72 with the iPhone/Korean adapters and menu cleanup from 4.0.60.
`NGB_IOS_PERF_PROBE=ON` enables observation only; frame matching, safety skips, shader scheduling,
cache/loading and presentation decisions remain unchanged. The observer patch is applied after the
original graphics baseline check and reversed byte-for-byte by its regression check.

The Release-optimized diagnostic build appends `Documents/fps-diagnostics.log` in JSON lines.
The writer saves one aggregate per second on a separate thread. Producers update fixed atomic counters;
there are no per-draw dumps, extra vertex checks, screenshots, forced GPU waits or timing changes.
GPU timestamp queries use the original non-waiting query path. They are not zero-cost; timestamp support
and the sample count are logged. Synthetic observer cost is measured by the regression test; it is not
an estimate of the GPU timestamp cost on the phone.

Important fields:
- `game_fps` counts original flips; `shown_fps` includes interpolation. Rates use the original
  half-second FPS windows, whose duration is recorded separately from the writer's sampling window.
- `skip_camera`, `skip_unpaired`, `skip_empty` explain rejected matches; reasons can overlap.
  `single_picture` identifies settings with no extra picture; `skip_broken` is an incomplete recording.
- `interp_cpu_*` and `interp_wall_*` cover Match/replay/held-picture work, not the entire original frame.
- `gpu_span_*` are command-buffer timestamp spans, not utilization or isolated whole-frame timings.
  `gpu_supported=0` or no samples means GPU time is unavailable, never proof of zero GPU load.
- `gpu_wait_*`, synchronous and asynchronous shader/pipeline compile times, missing-pipeline draws,
  queue size, thermal state (0 nominal, 1 fair, 2 serious, 3 critical), low-power and foreground state.
- `writer_cpu_previous_ms` measures the last aggregate's writer CPU time. Counter snapshots can
  straddle a window boundary; compare trends and session totals. Background gaps are explicitly marked.

Reproduce the combat slowdown with Full Rate, continue for at least five seconds, then send the log.
A short Off comparison in the same scene is useful. No saves or game memory are written to the log.

Delivery: the workflow first creates an **unpublished draft** and tests anonymous page and public-text
asset access (both must return 404). It checks draft/source identity again before uploading the IPA,
verifies the final asset size/hash and anonymous denial, removes the test asset, and never publishes.
The owner can download the single `.ipa` while signed into GitHub. An encrypted artifact remains a
backup transport; no raw IPA is uploaded as a public Actions artifact.
