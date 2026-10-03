# R3 selected-radio recovery

Implements the already approved R-R3-27 and R-R3-29 requirements in the
[R3 plan](2026-09-20-remote-daemon-r3-plan.md). Execute with
yonder-cost-aware-execution; use meaningful boundary tests and one integrated
review. Receive-only policy remains in force throughout recovery.

## Ownership and behavior

* P2 detects established **all inbound UDP** silence using the three-second
  watchdog in Thetis `ChannelMaster/network.c:655–666`, v2.10.3.15. Accepted
  packets must belong to the selected address and a valid negotiated input
  role. Initial no-IQ timeout remains separate. Valid status traffic alone
  keeps this watchdog alive: no unsourced IQ-only timeout is introduced.
* On loss, P2 stops its producers, clears transmit intent, sends run=0/MOX=0,
  closes ingress and reports one `NoDataTimeout` and `LinkLost`. It does not
  choose a radio or retry. The existing owned Nereus DSP/audio teardown takes
  the place of Thetis's fixed ChannelMaster zero-buffer injection.
* DaemonApp keeps the authenticated control plane alive. Discovery runs on a
  cancellable worker using the existing RadioDiscovery packet/parser path.
  Every attempt has a fresh result set. The configured MAC is authoritative;
  with an empty configured MAC, the first chosen identity becomes pinned for
  this daemon run. Other radios and busy discovery responses are not admitted.
* Retry delays are Nereus daemon policy, not a Thetis port: 1, 2, 4, 8, then
  at most 15 seconds between attempts. One discovery/connect attempt may be
  active at a time. Existing post-stop discovery quiet time still applies.
* A queued loss handler retires the existing model connection/DSP/audio and
  rebuilds them around the same slice objects. It must not overwrite live
  frequency, mode, pan membership or active selection. The first connection
  retains normal persisted-state loading. Stop or reconfiguration invalidates
  pending results and timers; old work cannot connect after cancellation.
  WDSP wisdom generation is non-interruptible: a stop during its nested setup
  wait cancels the eventual radio dial and defers model destruction until the
  existing job finishes. A second start is refused during that drain, so two
  global planning jobs cannot overlap.
* StationServer updates capabilities and MAC-scoped settings for clients
  already authenticated when a radio arrives. This is an incremental update,
  not a new handshake or a replay of object lifecycle events. Existing media
  controllers retire on non-Connected and subscribe again after fresh state.

## Interfaces and implementation sequence

- [x] RadioModel adds `connectToRadioPreservingSlices(const RadioInfo&)`,
  sharing the existing connection implementation and retaining the legacy
  `connectToRadio` entry point. The preserving entry point requires the same
  previously selected MAC. Primary-channel initialization uses the primary
  slice even when another slice is active.
- [x] RadioDiscovery supports worker interruption and synchronizes its
  process-wide monotonic quiet deadline. DaemonApp owns worker lifetime,
  pinned identity, retry backoff and generation cancellation.
- [x] P2 adds established ingress monitoring and terminal stop/reporting,
  without enabling its dormant independent reconnect timer.
- [x] StationServer sends current capabilities and scoped settings on late
  identity changes; the client keeps existing objects and session identity.
- [x] Integrate loss teardown, rebuilt FFT routing and media resumption.

Implementation, focused checks and the full 698-executable suite pass; see
[verification evidence](2026-09-20-remote-daemon-r3-verification/radio-recovery.md)
for matching installation evidence and the open hardware acceptance gates.

## Verification

Use real loopback UDP for P2 first-IQ/established-loss/status-only/late-packet
tests. Compress deadlines only through test-only seams. Prove no continued
egress or keyed stop packet after loss, and no loss after intentional stop.
Use the real RadioModel/WDSP synchronous-init test seam for preservation and
daemon late arrival. Inject discovery results at the discovery boundary to
exercise wrong identity, absence, cancellation and subsequent success without
probing an operator's LAN. Authenticate a real StationClient/StationServer
pair for late capabilities and settings scope. Check media retirement and
fresh resubscription, not just enum changes. Build matching focused targets,
then all_tests and the full suite against the integrated revision. Hardware
late-start and power-loss/resume remain pending until receive-only smoke tests.

## Separate control-parity follow-up

R-R3-21 still needs an audit of the P2 Setup “Network WDT” control. The current
`P2RadioConnection::setWatchdogEnabled()` only stores `m_watchdogEnabled`, while
connection startup sets the separate wire field `m_wdt` to 1. This existing
control gap is not resolved by the host-side established-ingress watchdog in
this change. Establish the upstream/wire semantics before changing that toggle;
keep it separate from R4 transmit watchdog and starvation acceptance.
