# Receive-layout persistence foundation

Requirement: R-R3-34, Task 4f of the
[R3 plan](../2026-09-20-remote-daemon-r3-plan.md). Contract and remaining
integration: [receive-layout design](../2026-09-22-core-receive-layout-design.md).

## Delivered boundary

`ReceiveLayoutStore` validates and stages a bounded, versioned per-MAC JSON
manifest through AppSettings. It preserves stable IDs and list order,
frequency/mode, pan keys and the explicit RADE receive-audio owner, separates
missing/invalid state, and leaves the
prior value intact on validation failure. It performs no disk write until
the owner calls the existing atomic AppSettings save. No slice, DDC, decoder
or radio connection is constructed by this adapter.

The named general receive bounds are verified from pinned Thetis
`console.cs:15540,15552` at v2.10.3.15. The existing manual VFO entry behavior
and radio/DSP runtime source are unchanged. The adapter is not yet called by
daemon startup: this checkpoint does **not** claim receiver restoration works
across a Core process restart.

## Checks

Fresh focused build and CTest passed all three selected executables. The final
full build/run below also covers the subsequently added explicit RADE owner:

- `tst_receive_layout_store`: real file save/load across AppSettings lifetimes;
  reversed/noncontiguous IDs; MAC normalization and isolation; removed
  membership; shared/default pan; staging without a disk write; future,
  malformed and corrupt schemas; retained invalid bytes; non-destructive
  rejection of nonfinite/out-of-range staged data; RADE B ownership when A is
  USB; an explicit second owner among multiple RADE modes; refusal of ambiguous
  or stale ownership. Final Qt result: **41 passed, zero failed or skipped**.
- `tst_nnr_radio_persistence`: existing per-MAC/stable-ID NNR persistence and
  save failure/retry behavior.
- `tst_daemon_app`: existing daemon startup/shutdown and configured-count
  behavior remain intact while runtime restoration is pending.

Private focused logs: `r3-receive-layout-focused-build.log` and
`r3-receive-layout-focused-tests.log` under `~/.config/nereus/work/`.

The final desktop, `nereusd` and all test executables built successfully.
Full CTest: **719/719 passed**, zero failed, **269.24 seconds**, `-j6`.
Environment: macOS arm64, existing Ninja RelWithDebInfo `build-integration`.
Final-build-start load was 5.65 / 6.95 / 6.48; suite-start load was
9.94 / 8.55 / 7.16. macOS media analysis and audio services were also consuming
CPU during the run; this is verification evidence, not a performance comparison.
Twelve existing inner Qt cases remain skipped: host audio choices, a deferred
modal-menu case, optional PS form capture, and deferred native TX harness cases.
No new skip was introduced. The unchanged native WDSP source retains its
earlier independent review and sanitizer evidence.

Final logs under the same private directory:
`r3-receive-layout-final-build.log`, `r3-receive-layout-tests.log`,
`r3-receive-layout-skips.log` and `r3-receive-layout-load.log`.

## Remaining acceptance

Connect the adapter to side-effect-free offline hydration, actual resource
admission, the saved RADE receive owner's startup, coalesced saves and shutdown
flush. Reconcile first-responder discovery and already mirrored bootstrap
objects, including ordinary preferences, not just tuning fields. Publish
restoration/refusal status and give an empty GUI pan an actionable explanation.

The channel-0 special initialization must not activate a phantom A when the
restored stable-ID list begins with B or C. Distinct saved pans must preserve
their own-window allocation semantics. Tests must cover these through the
real daemon/session lifecycle, followed by the operator's two-pan receive-only
Core restart on the Rock. No hardware deployment or public Git operation was
performed for this foundation.
