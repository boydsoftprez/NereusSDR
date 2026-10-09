# Core shared spectrum engine: resize on a re-request

Execution: run with `crew` under `cost-aware-execution`. One task.

## Why

The Core runs one FFT engine per (receiver stream, tier) and shares it
among every display on it, on every device (Task 76). The size rule
(R-R3-01/R-R3-08, `DaemonMediaController::handleSubscribe`,
src/core/session/media/DaemonMediaController.cpp ~3125-3141 on main
0701d7dfc): a request sizes its engine only while it is the engine's only
subscriber; beside others it is granted the largest `sourceFftSize` among
them. The engine is re-sized for the others only when a display leaves
(`rebalanceSourceAfterDeparture`, ~5697-5789), and then only upward.

So when every display on an engine asks again at a new size, each is
granted the others' old size, and the engine never moves. A remote window
subscribes its pan and its slice's mini display at the stored FFT size
(`DisplayFftSize`, a Station key the Core holds). Bench 2026-10-09 (ANAN G2
via the Core, Core log `nereussdr-20261009-133715-994-000003.log`, client
log `nereussdr-20261009-114650-767-000001.log` 15:34:46-15:36:12): while
JJ moved the FFT size slider, pan-0's wide grants read "FFT 131072 (wide
tier) ... limit shared" and the engine replanned between 65536, 131072 and
262144. Lowering the size from 262144 to 32768 with both displays on the
wide engine leaves it at 262144 (2.7 s of samples per transform at 96 kHz)
until one display changes tier or closes.

## Rule (the fix)

On a re-request (the endpoint already exists on the **same** source key),
the engine may move, but only in a way no other display objects to. For
each other display j on the engine, with current engine size `E` and its
own clamped request `Rj = min(j.grant.requestedFftSize, FFTEngine::maximumFftSize())`,
j accepts any size between `E` and `Rj` inclusive (the engine moves toward
what j asked for, never past it). The re-requester's clamped request is
`Ri`. The new size is

    L  = max over j of min(E, Rj)
    H  = min over j of max(E, Rj)
    E' = clamp(Ri, L, H)

`[L, H]` always contains `E`. Consequences:

- A display still at exactly its own request holds the engine (`L = H = E`
  for that j): unchanged R-R3-01 behaviour, no pan's spectrum changes to
  satisfy another pan's request.
- When no other display asks for the current size (the engine is stale),
  it moves toward the re-requester, and every other display ends up at
  least as close to its own request as before. Both the lower and the
  raise case resolve once the second display asks again.
- A new display (no existing entry) or a tier change (existing entry on a
  different source) keeps today's rule exactly. Departures keep
  `rebalanceSourceAfterDeparture` exactly.

All of `E`, `Ri`, `Rj` are powers of two, so `E'` is one too.

When `E' != E`: every display on the engine (every member device, as
`forEachSharedEndpoint` walks them) records `sourceFftSize = E'`, the
re-requester's grant is `E'`, and `reconcileSource(key)` reconfigures the
engine. The other displays' grants (size, pixels, reason) are then
renewed from the next frame by `configureEndpointFromFrame` (~4548-4580),
as on a departure regrant. If `reconcileSource` fails, every changed entry
is restored, as `rebalanceSourceAfterDeparture` restores its regrants, and
the request is refused as the existing failure path refuses it.
Other member devices get `refreshDisplayBudgetPacer()` as in the departure
path. The display charge: follow the departure path's handling of
`request.pixels` / `displayCost` / `chargeCoversRequest` for the other
displays; a display never gains pixels its admitted charge does not cover.

Decimation sharing is out of scope (unchanged).

## Global Constraints

- Read CLAUDE.md and CONTRIBUTING.md first. C++20/Qt6. No raw new/delete,
  no `#define` constants, braces on all control flow, `kPascalCase`
  constants, `m_camelCase` members.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- NereusSDR's own code, no port; no attribution header change.
- Do not change the tier rule, `grantReason`, `kDisplayFftPlanMaxSize`,
  `rebalanceSourceAfterDeparture`'s behaviour, or the new-display path.
- Build and run single tests only, per docs/development/fast-test-loop.md.
  Test binaries run with `QT_QPA_PLATFORM=offscreen` prefixed on the
  command, never exported over ctest.
- Commits GPG-signed (never `--no-gpg-sign`). No Claude co-author trailer.

## Task 1: Re-request resizes a stale shared engine

Files:
- src/core/session/media/DaemonMediaController.cpp (and .h if a helper
  needs declaring): `handleSubscribe`'s shared-size computation.
- tests/tst_daemon_media_controller.cpp: new slot(s), using the existing
  `Harness`, `tieredSubscription`, `messageFor`, `messageCount`,
  `spectrumGrant` helpers (see `sharedEngineKeepsOtherPanWhileNeighbourChurns`
  ~5173 and `sharedEngineRegrantsEverySurvivorWhenItsSizerLeaves` ~3756).

Steps:
1. Write the reproduction test first and run it against the unchanged code
   to prove it fails: two displays on one wide engine, both at 8192 (the
   engine at 8192); E1 re-requests at 2048, then E2 re-requests at 2048.
   Expect the engine (both grants, via `spectrumGrant` and the renewed
   contexts' `grantedFftSize`) at 2048. Today it stays at 8192.
2. Implement the rule above.
3. Add rows (data-driven or separate slots, implementer's call):
   - Lower: as step 1 -> 2048 for both, `limit` "none" for both.
   - Raise: both at 2048, both re-request 8192 -> 8192 for both.
   - Holder keeps the engine: E1 at 1024 (sole, so it sized the engine);
     E2 joins at 4096 (granted 1024, "shared"); E2 re-requests 4096 again,
     and again at 2048: engine stays 1024, E1's context is not renewed
     (same `contextGeneration`, the `e1Untouched` pattern).
   - Partial: E1 asks 8192 (engine 8192), E2 joins at 4096 (granted 8192,
     reason none); E1 re-requests 2048: engine goes to 4096 (`L` is E2's
     4096), E1 granted 4096 (more than it asked, so `grantReason` says
     "none"), E2 at its own 4096 "none".
   - In step 1 and the raise row, after only the first display asks
     again, the engine has not moved yet (the other display still holds
     it at its own request) and the first is granted the old size,
     "shared"; assert that intermediate state too.
   - Two devices (Task 76), if the harness supports a second member
     device; otherwise say so in the report.
4. Run the new slots and every existing shared-engine slot in
   tst_daemon_media_controller (`sharedEngine*`,
   `grantReportsLargestSizeSharedEngineAndSourceBins`,
   `regrantAfterNeighbourLeavesStaysWithinAdmittedCharge`,
   `extendedPermissionFirstCaptureAndSharedEndpointLifetimes`), then the
   whole tst_daemon_media_controller binary once. If an existing test
   asserted the old stuck behaviour, report it with what it asserted;
   do not weaken it.
5. Build the nereusd and NereusSDR targets.

Acceptance: the rows above pass; the reproduction failed before the fix;
existing shared-engine tests pass unchanged.

Hardware: pending (JJ's remote window on the G2 Core: lower FFT size from
262144 to 32768 while zoomed out; the Core log replans to 32768).

## Addendum (2026-10-09, after the bench): two more causes

JJ's remote window at 17:15 (Setup > Display > Spectrum Defaults): size
slider at notch 4 (65536), Hz/bin Target Off, Bin Width 2.930 Hz (768 kHz),
FFT Size readout 262144. Client log 17:07:53 "Remote spectrum grant for
pan-0: FFT 262144 (wide tier), 1068 of 1068 points, limit none": the pan
asked for less and was handed the engine another display held. Task 1 alone
does not unstick it, for two reasons.

**A. The window never asks again when a spectrum setting changes.**
`RemoteMediaController` reads `DisplayFftSize`, `DisplayFftWindow`,
`DisplayHzPerBinTarget` and `DisplaySpectrumFps` from `AppSettings` when it
builds a request (`plannedFftSize` ~604, `requestMini` ~1059), but nothing
calls `refreshSubscriptions()` when one of them changes. The Hz/bin box
happens to force one (`sw->requestAutoZoomReplan()`,
src/gui/setup/DisplaySetupPages.cpp ~685); the FFT size slider (~695-765)
and the window combo do not. A change from another device
(`SettingsProxy::applyRemoteValue`, src/core/settings/SettingsProxy.cpp
~298) is cached silently, and a reconnect snapshot likewise. The slider
handler then calls `refreshGrantedReadouts()`, which writes the OLD grant
back into the FFT Size and Bin Width readouts, so the slider looks dead.

**B. A departure never shrinks a display granted more than it asked for.**
`rebalanceSourceAfterDeparture` (src/core/session/media/DaemonMediaController.cpp
~5773-5870) re-grants only entries whose `grant.reason` is `SharedEngine`
(granted less than requested) and only upward. An entry granted MORE than
it asked (reason none) keeps the departed holder's size forever: e.g. the
mini display holds the wide engine at 262144, the pan re-asks at 65536 and
is granted 262144, then the mini changes tier (a departure from wide) and
the pan is left alone at 262144.

## Task 2: A departure regrants every remaining display to its own request

Files: DaemonMediaController.cpp (`rebalanceSourceAfterDeparture`),
tests/tst_daemon_media_controller.cpp.

Rule: after a departure, with remaining displays j and clamped requests
`Rj = min(j.grant.requestedFftSize, FFTEngine::maximumFftSize())`, the
engine runs at `E' = max over j of Rj`. Every remaining entry whose
`sourceFftSize != E'` records `E'` (up or down); `reconcileSource(key)`
reconfigures; renewed contexts carry the new grants via
`configureEndpointFromFrame` as today. No display ends below its own
request, so this never conflicts with Task 1's holder rule. Keep the
existing pixel/charge handling for entries that grow (a display never gains
pixels its admitted charge does not cover); an entry that shrinks keeps its
pixels and charge. Keep the decimation path, the rollback on a failed
`reconcileSource`, and the other members' `refreshDisplayBudgetPacer()`.
Update the comment "so a later departure among them does not shrink the
engine under the others" to state the new rule.

Steps: reproduction first, run red against the unchanged code: E1 (holder)
at 8192 sizes the engine; E2 joins asking 2048 (granted 8192, reason none);
E1 leaves (unsubscribe, and separately a tier change by re-requesting E1 at
a size that moves it to the other tier, if the harness can express it).
Expect E2 regranted 2048 (`spectrumGrant`, renewed `grantedFftSize`), engine
reconfigured to 2048. Also: three displays (8192 holder, 4096, 2048) and the
holder leaves -> 4096 for both. Existing `sharedEngine*` and
`regrantAfterNeighbourLeavesStaysWithinAdmittedCharge` slots pass
unchanged; if one asserted the old upward-only behaviour, report it with
what it asserted, do not weaken it.

## Task 3: A remote window asks again when a spectrum setting changes

Files: src/core/settings/SettingsProxy.{h,cpp}, src/gui/RemoteMediaController.{h,cpp},
src/gui/MainWindow.cpp (wiring next to the existing `snapshotApplied`
connects ~4271), src/gui/setup/DisplaySetupPages.cpp, tests.

1. `SettingsProxy` gains `void valueChanged(const QString& key)`, emitted
   when the cached value for `key` actually changes: from `setValue()`
   (this window's own write), `applyRemoteValue()` (another device or the
   Core; not for an echo that leaves the value unchanged),
   `applyRemoteRemoval()` and `applyRejection()`. No GUI include (R1).
2. `RemoteMediaController` gains a public method that re-asks the Core for
   every pan and mini display (it calls `refreshSubscriptions()`; the mini
   path already re-sends when its request differs). MainWindow connects the
   proxy's `valueChanged` filtered to the four keys above, and
   `snapshotApplied`, to it. Coalesce with a zero-delay single-shot so a
   burst of keys (a snapshot, a slider drag) sends one request per display.
3. DisplaySetupPages, remote window only: the FFT size slider and the window
   combo no longer call `refreshGrantedReadouts()` from their change
   handlers. The readouts show the value the operator chose until the
   Core's next grant arrives (`remoteSpectrumGrantChanged`, ~1433), which
   then shows what the Core runs. Leave the Hz/bin box's existing
   `requestAutoZoomReplan()` (the local engine needs it); it is harmless
   beside the new path.
4. Tests: SettingsProxy (signal fires once per real change from each path,
   not for an unchanged echo); RemoteMediaController or the nearest existing
   remote-window test harness: changing `DisplayFftSize` through the proxy
   produces a new subscribe for the pan and the mini at the new size, with
   no zoom or resize. Find the existing harness first
   (tests/tst_remote_media_controller*.cpp or similar) and reuse it.

Hardware (both tasks): JJ's window on the G2 Core at 768 kHz: move the
slider 65536 -> 16384 -> 65536 while zoomed out; the client log grants and
the Core's replans follow each notch, and the FFT Size readout matches.

## Task 3 withdrawn (cause A did not hold)

The implementer's re-ask test passed on the unchanged code. A remote window
already re-asks: `RemoteMediaController` runs a 100 ms planner timer
(kPlannerIntervalMs, src/gui/RemoteMediaController.cpp ~1765-1767) wired to
`refreshSubscriptions()`, which rebuilds every pan and mini request from the
four settings and re-sends any that changed. So a change from this window,
another device or a snapshot reaches the Core within 100 ms. The bench FFT
Size readout of 262144 was the Core's true grant: the stuck engine of cause
B (fixed by Task 2) and the re-request rule (Task 1). The readout keeps
showing the grant; showing the chosen size instead would go stale when the
Core grants the same size again. Task 3 is dropped; nothing of it is
committed.
