# R3 CPU-adaptive spectrum implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the integration worktree (lane A).

**Goal:** when the Core computer runs short of CPU, audio and receive processing
keep priority: the Core lowers spectrum frame rate, then detail, on background
pans first, says why through the pan's status, and restores quality when
headroom returns. It acts before the noise-reduction step-back needs to.

**Architecture:** one shared sampler already caches each receiver's DSP load
(`RadioModel::receiverDspLoad`); the Core's host CPU sampler becomes shared the
same way. A pure `DisplayLoadGovernor` turns those readings into display-budget
limits with hysteresis and a floor, and the Core applies them through the
existing display-budget path (`StationServer::setDisplayBudgetLimits`, grant
reasons, active-pan priority, restore). A new capability field carries the reason
to apps that understand it; the app's pan status shows it (the words come from
the pan status builder in the user wording plan).

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), `DaemonTelemetryController`,
`HostTelemetrySampler`, `DaemonMediaController`, `DisplayBudget`,
`StationServer`, `StationCapabilities`, `StationClient`, `RemoteMediaController`.

**Spec:** R3 plan requirement R-R3-08 (CPU side: explicit capacity downgrade),
R-R3-37 (display budget), R-R3-40 (DSP load); operator answer of 2026-09-23 (R3
question 4: adapt automatically, audio first, background pans first, reason in
the pan status, restore on headroom). Scout notes with file:line:
`/Users/j.j.boyd/.config/nereus/work/r3-next-control-ui-scout-2026-09-23.md`
section 1.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Protocol: gate the new reason with its own capability field; do not change
  `kSessionProtocolMinor`. Older apps require exactly five budget fields
  (`StationCapabilities.cpp:220`): parse the new field separately and keep the
  older shape byte-for-byte (golden). New spectrum limit reason strings are sent
  only to apps that declare support (older apps reject unknown limit strings,
  `RemoteSpectrumContext.cpp:73-86, 214-219`).
- Operator wording: plain user words ("Core busy"); no internal terms.
- Telemetry and governor readers never take a DSP lock and never reset another
  reader's interval.
- Local (desktop) operation unchanged.
- Tests run off-screen. Build exact targets, then
  `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No unfiltered suite.
  Never build the `NereusSDR` target. No hardware.

## What already exists

- Display-budget machinery: app-side reduction order and restore
  (`RemoteDisplayAllocator.cpp:123-131`, floors 10 fps / 256 px at :20-21, pause at
  :224-235; `RemoteMediaController::refreshBudgetSubscriptions` :1357 replans per
  limits generation); Core side `StationServer::setDisplayBudgetLimits`
  (`StationServer.cpp:1229-1242`), `DaemonMediaController` pacer re-rate
  (:314-318, :621), `spectrumAdmissionFits` (:580), `DisplayBudgetPacer`
  (`DisplayBudget.cpp:224, 277`), grant reasons (`SpectrumEndpoint.h:43-51`,
  `DaemonMediaController.cpp:172-186`), `rebalanceSourceAfterDeparture` (:1783).
  Budget mode exists only when nereusd's config sets both limits
  (`DaemonApp.cpp:455-457`).
- `RadioModel::receiverDspLoad(sliceId)` returns the cached per-receiver snapshot
  every 500 ms (`src/models/ReceiverDspLoadSampler.h`); `idle` is not proof of no
  load.
- `HostTelemetrySampler` is private to `DaemonTelemetryController`
  (`DaemonTelemetryController.cpp:38-41`, `sampleNow` :293-312), Linux only, CPU %
  since the previous call (so a second reader would corrupt it).
- The NNR step-back governor acts at receiver load 0.90 held for 2 s.

## Task 1: The Core lowers spectrum first when it is busy

**Requirements:** R-R3-08, R-R3-37, R-R3-40.

**Files:**
- Create: `src/core/session/media/DisplayLoadGovernor.{h,cpp}` (pure),
  `tests/tst_display_load_governor.cpp`
- Modify: `src/core/daemon/DaemonTelemetryController.{h,cpp}` and
  `src/core/daemon/HostTelemetrySampler.{h,cpp}` (one shared host sampler whose
  cached reading telemetry and the governor both read),
  `src/core/daemon/DaemonApp.cpp` (own the governor; with adaptation on, always
  advertise a budget: the configured limits or a computed ceiling of eight pans
  at maximum cost plus PureSignal, so apps are in budget mode from the start),
  `src/core/daemon/DaemonConfig.{h,cpp}` and `packaging/nereusd.conf.sample`
  (`display_adaptive = on|off`, default `on`), `src/core/session/StationServer.cpp`,
  `src/core/session/StationCapabilities.{h,cpp}` (`displayBudgetReason`),
  `src/core/session/StationClient.{h,cpp}` (expose the reason),
  `src/gui/RemoteMediaController.cpp` (carry the reason into the pan's state; the
  words are the pan status builder's), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_display_load_governor.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_remote_media_controller.cpp`, `tests/tst_display_budget_contract.cpp`,
  `tests/tst_daemon_telemetry.cpp`, `tests/tst_daemon_config.cpp`

**Interfaces:**
- Consumes: `RadioModel::receiverDspLoad(sliceId)`, the shared host sampler.
- Produces: `DisplayLoadGovernor` (inputs: highest receiver load, late blocks and
  input delay per interval, system CPU %, the current accepted display charge;
  output: limits and a reason, none or Core busy); capability field
  `displayBudgetReason`.

**Acceptance:**
- Step down when the highest receiver load is at least 0.75 or system CPU at least
  85 % for 2 s (named constants, below the NNR step-back's 0.90 so spectrum yields
  first); restore one step after 10 s under 0.60 load and 70 % CPU; never below
  the floor (the PureSignal charge plus one active pan at 10 fps and 256 px); no
  change without measurements (macOS, no receivers, idle).
- Each step publishes a new limits generation through
  `StationServer::setDisplayBudgetLimits`; the existing app-side order applies
  (background frame rate, background detail, then the active pan); restore brings
  back the requested quality and clears the reason.
- With adaptation on and no configured limits, the Core advertises a computed
  ceiling so apps plan in budget mode; with `display_adaptive = off` behaviour is
  exactly today's.
- Apps that declare support receive `displayBudgetReason`; older apps receive
  exactly today's capability shape (golden) and no new limit strings.
- The shared host sampler gives telemetry and the governor the same reading; the
  telemetry values are unchanged from today's (existing tests).
- Tests: governor with an injected clock and inputs (down, hold, floor, restore,
  no inputs); daemon media controller lower then raise limits (pacer, admission,
  capability publish); four pans in the remote media controller (a Core-busy cut
  reduces background pans first, the active pan last, restore clears it);
  capability golden for older apps; config default and off.

**Verification:** capacity behaviour, unit plus media controller integration.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_display_load_governor tst_daemon_media_controller tst_remote_media_controller tst_display_budget_contract tst_daemon_telemetry tst_daemon_config nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_display_load_governor|tst_daemon_media_controller|tst_remote_media_controller|tst_display_budget_contract|tst_daemon_telemetry|tst_daemon_config)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): four pans plus Premium NNR on the Rock;
background pans slow first, NNR steps back only if still needed; the thresholds
are revisited with the measurements.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Shared host sampler and governor with tests.
- [ ] **Step 2:** Core wiring, capability, app state; run the commands; commit.
