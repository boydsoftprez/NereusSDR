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
