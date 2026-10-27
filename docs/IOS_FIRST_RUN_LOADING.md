# First-run cache preparation

The 2.74 device trace maintained 60.004 game frames per second while the
loading-ready gate stayed active for 73–74 seconds. The title was still
preparing data, rather than spending this whole interval waiting for input.
After readiness, it waited approximately another eight seconds for the
player to continue. Do not attribute the entire stage-3 duration to input.

The original cache creator `0x0031C660` writes AFS data to the utility drive
(`Z:`, mapped to the title's disposable Cache directory). Its packet loop
at `0x0031C750` sleeps **50 ms before each 64 KiB packet**, limiting that loop
to at most 1.25 MiB/s even before decoding and file I/O. Cache existence is
checked before this work, explaining why subsequent launches can skip it.
The trace and source identify this throttle as a concrete bottleneck;
the trace did not yet count packet iterations, so the exact contribution
to the reported wait still needs the next device measurement.

The iOS-only code-generation adapter now calls a host delay policy at that
one pinned boundary. An original 50 ms wait becomes 1 ms only in active
state zero. Pause, pause requests, stop requests, error backoffs, other sleep
call sites, decoded bytes, write calls and completion conditions remain
unchanged. The original macOS generation does not run this adapter.
`ios/cache_copy_policy.h` is the small native policy; tests check its scope,
and the adapted real private worker is compiled before production builds.
No cache pre-seeding, game-state skipping, save edits, or output-format
changes are involved.

Optional investigation builds add `ui-cache-wait` records to `ui-layout.log`:
total packet polls, cumulative original requested milliseconds, cumulative
effective milliseconds and worker state. Records are limited to one per
second or state change, with at most 240 samples. Existing `ui-load-state`
and `ui-load-font` samples remain available. All of these are absent from
builds with movie/UI diagnostics disabled. Real first-cache timing after
the change remains unverified until a device trace is supplied.
